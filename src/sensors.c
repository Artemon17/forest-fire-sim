#define _POSIX_C_SOURCE 200809L

#include "sensors.h"
#include "messages.h"

#include <stddef.h>

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
            if (w->state[r * C + c] == ST_BURNING) count++;
        }
    }
    return count;
}

void sensors_observe(World *w) {
    int R = w->map->rows, C = w->map->cols;
    int rad = w->cfg->sensor_radius;
    int n_cells = R * C;

    for (int i = 0; i < w->sensors_count; i++) {
        const Sensor *s = &w->sensors[i];
        unsigned char *reported = w->sensor_reported + (size_t)i * n_cells;

        for (int r = s->r - rad; r <= s->r + rad; r++) {
            if (r < 0 || r >= R) continue;
            for (int c = s->c - rad; c <= s->c + rad; c++) {
                if (c < 0 || c >= C) continue;
                int idx = r * C + c;
                if (w->state[idx] != ST_BURNING) continue;
                if (reported[idx]) continue;

                reported[idx] = 1;
                messages_send(w, i, r, c);
            }
        }
    }
}