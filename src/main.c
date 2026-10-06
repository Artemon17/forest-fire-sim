#include "config.h"
#include "map.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    const char *config_path = "data/conditions.conf";
    const char *map_path    = NULL;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--conditions") && i + 1 < argc) {
            config_path = argv[++i];
        } else if (!strcmp(argv[i], "--map") && i + 1 < argc) {
            map_path = argv[++i];
        } else {
            fprintf(stderr, "Неизвестный аргумент: %s\n", argv[i]);
            return 2;
        }
    }

    Config cfg;
    if (config_load(config_path, &cfg) != 0) {
        fprintf(stderr, "Не удалось загрузить конфиг '%s'\n", config_path);
        return 3;
    }
    config_dump(&cfg);
    printf("\n");

    if (!map_path) {
        printf("Карта не указана (--map). Пропускаем.\n");
        return 0;
    }

    MapData map;
    if (map_load(map_path, &map) != 0) {
        fprintf(stderr, "Не удалось загрузить карту '%s'\n", map_path);
        return 4;
    }
    map_dump(&map);
    map_free(&map);
    return 0;
}