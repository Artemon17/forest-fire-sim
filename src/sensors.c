#define _POSIX_C_SOURCE 200809L

#include "sensors.h"

static int abs_int(int x) { return x < 0 ? -x : x; }

/* Сколько горящих клеток в зоне видимости датчика. */
int sensor_burning_in_view(const World *w, int sensor_idx) {
    if (sensor_idx < 0 || sensor_idx >= w->sensors_count) return 0;
    const Sensor *s = &w->sensors[sensor_idx];
    int rad = w->cfg->sensor_radius;
    int R = w->map->rows, C = w->map->cols;
    int count = 0;

    for (int r = s->r - rad; r <= s->r + rad; r++) {
        if (r < 0 || r >= R) continue;
        for (int c = s->c - rad; c <= s->c + rad; c++) {
            if (c < 0 || c >= C) continue;
            if (abs_int(r - s->r) > rad) continue;   /* не нужно, но для ясности */
            if (abs_int(c - s->c) > rad) continue;
            if (w->state[r * C + c] == ST_BURNING) count++;
        }
    }
    return count;
}

void sensors_observe(World *w) {
    for (int i = 0; i < w->sensors_count; i++) {
        int burning = sensor_burning_in_view(w, i);
        if (burning > 0) {
            const Sensor *s = &w->sensors[i];
            log_add(&w->log, w->tick,
                    "Датчик #%d (%d,%d): видит %d гор. клеток",
                    s->id, s->r + 1, s->c + 1, burning);
        }
    }
}