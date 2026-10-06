#include "config.h"
#include "term.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    const char *config_path = "data/conditions.conf";

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--conditions") && i + 1 < argc) {
            config_path = argv[++i];
        }
    }

    Config cfg;
    if (config_load(config_path, &cfg) != 0) {
        fprintf(stderr, "Не удалось загрузить конфиг '%s'\n", config_path);
        return 3;
    }

    config_dump(&cfg);
    return 0;
}