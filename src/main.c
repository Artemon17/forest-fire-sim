#define _POSIX_C_SOURCE 200809L

#include "config.h"
#include "map.h"
#include "world.h"
#include "render.h"
#include "sim.h"
#include "term.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ─── Разбор аргументов ───────────────────────────────── */

typedef struct {
    const char *config_path;
    const char *map_path;
    unsigned    seed;
    long        delay_ms;
    int         show_frames;
    int         use_ansi;
    int         scan_stations;

    int         st_r, st_c;         /* заданы вручную? */
    int         st_given;
} Options;

static void usage(const char *prog) {
    fprintf(stderr,
        "Использование: %s [опции]\n"
        "  --conditions FILE    файл конфига\n"
        "  --map FILE           файл карты\n"
        "  --station R C        поставить станцию в (R,C)\n"
        "  --station-scan       перебрать всех кандидатов из карты\n"
        "  --seed N             seed ГПСЧ\n"
        "  --delay MS           задержка между тактами\n"
        "  --quiet              не рисовать кадры\n"
        "  --no-ansi            отключить ANSI (для перенаправления в файл)\n",
        prog);
}

static int parse_args(int argc, char **argv, Options *o) {
    o->config_path  = "data/conditions.conf";
    o->map_path     = NULL;
    o->seed         = (unsigned)time(NULL);
    o->delay_ms     = 300;
    o->show_frames  = 1;
    o->use_ansi     = 1;
    o->scan_stations = 0;
    o->st_given     = 0;
    o->st_r         = 0;
    o->st_c         = 0;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--conditions") && i + 1 < argc)
            o->config_path = argv[++i];
        else if (!strcmp(argv[i], "--map") && i + 1 < argc)
            o->map_path = argv[++i];
        else if (!strcmp(argv[i], "--station") && i + 2 < argc) {
            o->st_r = atoi(argv[++i]);
            o->st_c = atoi(argv[++i]);
            o->st_given = 1;
        }
        else if (!strcmp(argv[i], "--station-scan"))
            o->scan_stations = 1;
        else if (!strcmp(argv[i], "--seed") && i + 1 < argc)
            o->seed = (unsigned)strtoul(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--delay") && i + 1 < argc)
            o->delay_ms = strtol(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--quiet"))
            o->show_frames = 0;
        else if (!strcmp(argv[i], "--no-ansi"))
            o->use_ansi = 0;
        else {
            fprintf(stderr, "Неизвестный аргумент: %s\n", argv[i]);
            usage(argv[0]);
            return -1;
        }
    }
    return 0;
}

/* ─── Один прогон с показом кадров ────────────────────── */

static SimResult run_interactive(World *w, const Options *o) {
    if (o->use_ansi) {
        term_install_signals();
        if (term_raw_mode() != 0) return RESULT_INTERRUPTED;
        term_hide_cursor();
    }

    render_frame(w);
    if (o->delay_ms > 0) term_sleep_ms(o->delay_ms);

    SimResult r = sim_finished(w);
    while (r == RESULT_RUNNING) {
        if (o->use_ansi) {
            int k = term_poll_key();
            if (k == 'q') { r = RESULT_INTERRUPTED; break; }
        }
        if (term_should_stop()) { r = RESULT_INTERRUPTED; break; }

        sim_tick(w);
        render_frame(w);
        if (o->delay_ms > 0) term_sleep_ms(o->delay_ms);

        r = sim_finished(w);
    }

    if (o->use_ansi) {
        term_show_cursor();
        term_reset_color();
        term_restore_mode();
        term_clear();
    }
    return r;
}

/* ─── Один прогон без кадров ──────────────────────────── */

static SimResult run_quiet(World *w) {
    return sim_run(w);
}

/* ─── Печать итога ────────────────────────────────────── */

static void print_result(const char *map_path, const char *config_path,
                         int st_r, int st_c,
                         SimResult r, int ticks, unsigned seed)
{
    printf("────────── ИТОГ ──────────\n");
    printf("Карта:      %s\n", map_path);
    printf("Условия:    %s\n", config_path);
    printf("Станция:    (%d,%d)\n", st_r + 1, st_c + 1);
    printf("Результат:  %s\n", sim_result_name(r));
    printf("Тактов:     %d\n", ticks);
    printf("seed:       %u\n", seed);
    printf("──────────────────────────\n");
}

/* ─── Режим одиночной станции ─────────────────────────── */

static int do_single(const Options *o, const MapData *map, const Config *cfg) {
    int st_r = o->st_given ? o->st_r : 0;
    int st_c = o->st_given ? o->st_c : 0;

    if (!o->st_given && map->station_candidates_count > 0) {
        st_r = map->station_candidates[0].r;
        st_c = map->station_candidates[0].c;
    }
    if (st_r < 0 || st_r >= map->rows || st_c < 0 || st_c >= map->cols) {
        fprintf(stderr, "Станция (%d,%d) вне карты\n", st_r, st_c);
        return 5;
    }

    World w;
    if (world_init(&w, map, cfg, st_r, st_c) != 0) return 5;

    srand(o->seed);

    SimResult r;
    if (o->show_frames) r = run_interactive(&w, o);
    else                r = run_quiet(&w);

    int ticks = w.tick;

    if (!o->show_frames)
        print_result(o->map_path, o->config_path, st_r, st_c, r, ticks, o->seed);

    world_free(&w);
    return 0;
}

/* ─── Режим перебора станций ──────────────────────────── */

typedef struct {
    int       r, c;
    SimResult result;
    int       ticks;
} ScanRow;

static int do_scan(const Options *o, const MapData *map, const Config *cfg) {
    if (map->station_candidates_count == 0) {
        fprintf(stderr, "В карте нет station_candidates для перебора\n");
        return 5;
    }

    ScanRow *rows = calloc((size_t)map->station_candidates_count, sizeof(ScanRow));
    if (!rows) return 5;

    printf("Перебор %d позиций станции (seed = %u)...\n\n",
           map->station_candidates_count, o->seed);

    for (int i = 0; i < map->station_candidates_count; i++) {
        int sr = map->station_candidates[i].r;
        int sc = map->station_candidates[i].c;

        World w;
        if (world_init(&w, map, cfg, sr, sc) != 0) {
            rows[i].r = sr; rows[i].c = sc;
            rows[i].result = RESULT_INTERRUPTED;
            rows[i].ticks = -1;
            continue;
        }
        srand(o->seed);   /* одинаковый поток случайностей для всех */
        SimResult r = sim_run(&w);
        rows[i].r = sr;
        rows[i].c = sc;
        rows[i].result = r;
        rows[i].ticks  = w.tick;
        world_free(&w);

        printf("  (%2d,%2d): %-20s %d тактов\n",
               sr + 1, sc + 1, sim_result_name(r), w.tick);
    }

    /* лучший результат */
    int best = -1;
    for (int i = 0; i < map->station_candidates_count; i++) {
        if (rows[i].result != RESULT_EXTINGUISHED) continue;
        if (best < 0 || rows[i].ticks < rows[best].ticks) best = i;
    }

    printf("\n────────── ИТОГ ПЕРЕБОРА ──────────\n");
    if (best < 0) {
        printf("Ни одна позиция не ликвидировала пожар\n");
    } else {
        printf("Лучшая станция: (%d,%d), ликвидация за %d тактов\n",
               rows[best].r + 1, rows[best].c + 1, rows[best].ticks);
    }
    printf("seed: %u\n", o->seed);
    printf("────────────────────────────────────\n");

    free(rows);
    return 0;
}

/* ─── main ────────────────────────────────────────────── */

int main(int argc, char **argv) {
    Options o;
    if (parse_args(argc, argv, &o) != 0) return 2;

    Config cfg;
    if (config_load(o.config_path, &cfg) != 0) return 3;

    if (!o.map_path) {
        fprintf(stderr, "Укажите карту через --map\n");
        return 2;
    }

    MapData map;
    if (map_load(o.map_path, &map) != 0) return 4;

    int rc = 0;
    if (o.scan_stations)
        rc = do_scan(&o, &map, &cfg);
    else
        rc = do_single(&o, &map, &cfg);

    map_free(&map);
    return rc;
}