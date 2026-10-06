#define _POSIX_C_SOURCE 200809L

#include "render.h"
#include "term.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define FRAME_BUF (1 << 16)   /* 64 KiB хватит с запасом */

static const char *color_for(char sym) {
    switch (sym) {
        case 'F': return C_FIRE;
        case '#': return C_BURNT;
        case 'w': return C_WATER;
        case '_': return C_FIREBRK;
        case '.': return C_SENSOR;
        case 'S': return C_STATION;
        case 't': return C_FOREST;
        case '"': return C_GRASS;
        default:
            if (sym >= '1' && sym <= '9') return C_TEAM;
            return C_RESET;
    }
}

void render_frame(const World *w) {
    static char buf[FRAME_BUF];
    size_t p = 0;

    #define APPEND(...) do { \
        int _n = snprintf(buf + p, sizeof(buf) - p, __VA_ARGS__); \
        if (_n > 0) p += (size_t)_n; \
        if (p >= sizeof(buf)) p = sizeof(buf) - 1; \
    } while (0)

    /* подсчитать состояния */
    int burning = 0, burnt = 0, ext = 0;
    int n = w->map->rows * w->map->cols;
    for (int i = 0; i < n; i++) {
        if      (w->state[i] == ST_BURNING)      burning++;
        else if (w->state[i] == ST_BURNT)        burnt++;
        else if (w->state[i] == ST_EXTINGUISHED) ext++;
    }

    APPEND(ESC_HOME);
    APPEND("Такт %d | Ветер: %s %d | Огонь: %d | Потухло: %d | Выгорело: %d\n",
           w->tick,
           wind_dir_name(w->cfg->wind_direction),
           w->cfg->wind_power,
           burning, ext, burnt);

    APPEND("────────────────────────────────────────\n");

    /* заголовок столбцов */
    APPEND("    ");
    for (int c = 1; c <= w->map->cols; c++)
        APPEND("%2d ", c);
    APPEND("\n");

    /* карта */
    for (int r = 0; r < w->map->rows; r++) {
        APPEND("%2d |", r + 1);
        for (int c = 0; c < w->map->cols; c++) {
            CellView cv = world_cell_view(w, r, c);
            char sym = world_display_symbol(&cv);
            const char *col = color_for(sym);
            APPEND("%s%c%s  ", col, sym, C_RESET);
        }
        APPEND("\n");
    }

    APPEND("────────────────────────────────────────\n");

    #undef APPEND

    ssize_t off = 0;
    while (off < (ssize_t)p) {
        ssize_t k = write(STDOUT_FILENO, buf + off, (size_t)(p - (size_t)off));
        if (k < 0) break;
        off += k;
    }
}