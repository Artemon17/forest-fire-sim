#include "config.h"
#include "map.h"
#include "world.h"
#include "render.h"
#include "fire.h"
#include "term.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char **argv) {
    unsigned seed = (unsigned)time(NULL);
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--seed") && i + 1 < argc)
            seed = (unsigned)strtoul(argv[++i], NULL, 10);
    }
    srand(seed);
    fprintf(stderr, "seed = %u\n", seed);

    const char *config_path = "data/conditions.conf";
    const char *map_path    = NULL;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--conditions") && i + 1 < argc)
            config_path = argv[++i];
        else if (!strcmp(argv[i], "--map") && i + 1 < argc)
            map_path = argv[++i];
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

    int st_r, st_c;
    if (map.station_candidates_count > 0) {
        st_r = map.station_candidates[0].r;
        st_c = map.station_candidates[0].c;
    } else { st_r = 0; st_c = 0; }

    World w;
    if (world_init(&w, &map, &cfg, st_r, st_c) != 0) {
        fprintf(stderr, "world_init не удался\n");
        map_free(&map);
        return 5;
    }

    srand((unsigned)time(NULL));

    term_install_signals();
    if (term_raw_mode() != 0) {
        world_free(&w); map_free(&map);
        return 1;
    }
    term_hide_cursor();
    term_clear();

    render_frame(&w);
    term_sleep_ms(700);

    while (!term_should_stop() && w.tick < cfg.end_max_ticks) {
        int k = term_poll_key();
        if (k == 'q') break;

        fire_spread(&w);
        render_frame(&w);
        term_sleep_ms(300);
    }

    term_show_cursor();
    term_reset_color();
    term_restore_mode();

    world_free(&w);
    map_free(&map);

    printf("Тактов: %d. Готово.\n", w.tick);
    return 0;
}