#include "config.h"
#include "map.h"
#include "world.h"
#include "render.h"
#include "sim.h"
#include "term.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char **argv) {
    const char *config_path = "data/conditions.conf";
    const char *map_path    = NULL;
    unsigned    seed        = (unsigned)time(NULL);
    long        delay_ms    = 300;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--conditions") && i + 1 < argc)
            config_path = argv[++i];
        else if (!strcmp(argv[i], "--map") && i + 1 < argc)
            map_path = argv[++i];
        else if (!strcmp(argv[i], "--seed") && i + 1 < argc)
            seed = (unsigned)strtoul(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--delay") && i + 1 < argc)
            delay_ms = strtol(argv[++i], NULL, 10);
        else {
            fprintf(stderr, "Неизвестный аргумент: %s\n", argv[i]);
            return 2;
        }
    }

    Config cfg;
    if (config_load(config_path, &cfg) != 0) return 3;

    if (!map_path) {
        fprintf(stderr, "Укажите карту через --map\n");
        return 2;
    }

    MapData map;
    if (map_load(map_path, &map) != 0) return 4;

    int st_r = 0, st_c = 0;
    if (map.station_candidates_count > 0) {
        st_r = map.station_candidates[0].r;
        st_c = map.station_candidates[0].c;
    }

    World w;
    if (world_init(&w, &map, &cfg, st_r, st_c) != 0) {
        map_free(&map);
        return 5;
    }

    srand(seed);
    fprintf(stderr, "seed = %u\n", seed);

    term_install_signals();
    if (term_raw_mode() != 0) {
        world_free(&w); map_free(&map);
        return 1;
    }
    term_hide_cursor();
    term_clear();

    render_frame(&w);
    term_sleep_ms(500);

    SimResult result = RESULT_RUNNING;
    while (!term_should_stop()) {
        int k = term_poll_key();
        if (k == 'q') { result = RESULT_INTERRUPTED; break; }

        sim_tick(&w);
        render_frame(&w);

        result = sim_finished(&w);
        if (result != RESULT_RUNNING) break;

        term_sleep_ms(delay_ms);
    }

    if (term_should_stop())
        result = RESULT_INTERRUPTED;

    term_show_cursor();
    term_reset_color();
    term_clear();
    term_restore_mode();

    /* Итог */
    printf("────────── ИТОГ ──────────\n");
    printf("Карта:      %s\n", map_path);
    printf("Условия:    %s\n", config_path);
    printf("Станция:    (%d,%d)\n", st_r + 1, st_c + 1);
    printf("Результат:  %s\n", sim_result_name(result));
    printf("Тактов:     %d\n", w.tick);
    printf("seed:       %u\n", seed);
    printf("──────────────────────────\n");

    world_free(&w);
    map_free(&map);
    return 0;
}