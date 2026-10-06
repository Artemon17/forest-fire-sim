#define _POSIX_C_SOURCE 200809L

#include "fire.h"

#include <stdlib.h>
#include <string.h>

/* Смещения 8 направлений в порядке N, NE, E, SE, S, SW, W, NW. */
static const int DR[8] = { -1, -1,  0, +1, +1, +1,  0, -1 };
static const int DC[8] = {  0, +1, +1, +1,  0, -1, -1, -1 };


/* Множитель ветра для направления соседа. */
static double wind_factor_for(const Config *cfg, int wind_dir, int dir) {
    int diff = (dir - wind_dir + 8) % 8;
    double along   = cfg->fire_wind_factor_along;
    double against = cfg->fire_wind_factor_against;
    double side    = cfg->fire_wind_factor_side;

    switch (diff) {
        case 0: return along;
        case 1: return (along + side) / 2.0;
        case 2: return side;
        case 3: return (side + against) / 2.0;
        case 4: return against;
        case 5: return (side + against) / 2.0;
        case 6: return side;
        case 7: return (along + side) / 2.0;
    }
    return side;
}

/* Базовая вероятность зажигания для типа растительности. */
static double base_prob(const Config *cfg, Terrain t) {
    switch (t) {
        case TERRAIN_GRASS:     return cfg->fire_base_prob_grass;
        case TERRAIN_TREE:      return cfg->fire_base_prob_tree;
        case TERRAIN_WATER:     return 0.0;
        case TERRAIN_FIREBREAK: return 0.0;
    }
    return 0.0;
}

/* Фактор возраста горения: чем дольше горит, тем сильнее «давит». */
static double burn_factor(const Config *cfg, int age) {
    double f = 1.0 + 0.1 * (double)age;
    (void)cfg;
    if (f > 2.0) f = 2.0;
    return f;
}

/* Воспламеняема ли клетка. */
static int can_ignite(const World *w, int r, int c) {
    Terrain t = w->map->cells[r * w->map->cols + c];
    if (t == TERRAIN_WATER || t == TERRAIN_FIREBREAK) return 0;
    FireState s = w->state[r * w->map->cols + c];
    if (s == ST_BURNING)      return 0;   /* уже горит */
    if (s == ST_BURNT && !w->cfg->fire_reburn_burnt) return 0;
    return 1;
}

/* Сколько тактов клетка должна гореть до полного выгорания.
 * Пока просто константа — потом можно завести в конфиг. */
int fire_burn_ticks(const World *w, int r, int c) {
    (void)w; (void)r; (void)c;
    return 6;   /* 6 тактов горения, потом # */
}

void fire_spread(World *w) {
    int rows = w->map->rows;
    int cols = w->map->cols;
    int n    = rows * cols;

    FireState *next = malloc((size_t)n * sizeof(FireState));
    if (!next) return;
    memcpy(next, w->state, (size_t)n * sizeof(FireState));

    int use8 = w->cfg->fire_spread_8dir;
    int n_dirs = use8 ? 8 : 4;
    /* для 4 направлений берём N, E, S, W — индексы 0, 2, 4, 6 */
    static const int D4[4] = { 0, 2, 4, 6 };

    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            int idx = r * cols + c;
            if (w->state[idx] != ST_BURNING) continue;

            int age = w->burn_age[idx];
            double bf = burn_factor(w->cfg, age);

            for (int k = 0; k < n_dirs; k++) {
                int d = use8 ? k : D4[k];
                int nr = r + DR[d];
                int nc = c + DC[d];
                if (nr < 0 || nr >= rows || nc < 0 || nc >= cols) continue;
                if (!can_ignite(w, nr, nc)) continue;

                double p = base_prob(w->cfg,
                                     w->map->cells[nr * cols + nc]);
                p *= wind_factor_for(w->cfg, (int)w->cfg->wind_direction, d);
                p *= bf;

                if (p <= 0.0) continue;
                if (p > 1.0)  p = 1.0;

                double roll = (double)rand() / (double)RAND_MAX;
                if (roll < p) {
                    next[nr * cols + nc] = ST_BURNING;
                }
            }

            /* источник: либо стареет, либо выгорает */
            int max_age = fire_burn_ticks(w, r, c);
            if (age + 1 >= max_age) {
                next[idx] = ST_BURNT;
            }
            /* burn_age обновим ниже отдельно */
        }
    }

    /* Применяем переходы и обновляем возраст */
    for (int i = 0; i < n; i++) {
        FireState prev = w->state[i];
        FireState nx   = next[i];

        if (prev == ST_BURNING && nx == ST_BURNT) {
            w->burn_age[i] = -1;         /* больше не горит */
        } else if (prev == ST_BURNING) {
            w->burn_age[i]++;
        } else if (prev != ST_BURNING && nx == ST_BURNING) {
            w->burn_age[i] = 0;          /* только что загорелся */
        }
        w->state[i] = nx;
    }

    free(next);
}