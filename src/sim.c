#define _POSIX_C_SOURCE 200809L

#include "sim.h"
#include "fire.h"
#include "sensors.h"
#include "messages.h"
#include "center.h"
#include "teams.h"

#include <string.h>

/* ─── Проверки состояния мира ─────────────────────────── */

static int count_burning(const World *w) {
    int n = w->map->rows * w->map->cols;
    int c = 0;
    for (int i = 0; i < n; i++)
        if (w->state[i] == ST_BURNING) c++;
    return c;
}

static int count_flammable_safe(const World *w) {
    int n = w->map->rows * w->map->cols;
    int c = 0;
    for (int i = 0; i < n; i++) {
        Terrain t = w->map->cells[i];
        if (t == TERRAIN_WATER || t == TERRAIN_FIREBREAK) continue;
        FireState s = w->state[i];
        if (s == ST_SAFE || s == ST_EXTINGUISHED) c++;
    }
    return c;
}


/* ─── Условия завершения ─────────────────────────────── */

SimResult sim_finished(const World *w) {
    if (count_burning(w) == 0) {
        return RESULT_EXTINGUISHED;
    }

    if (count_flammable_safe(w) == 0) {
        return RESULT_TERRITORY_EXHAUSTED;
    }

    /* 4. Лимит тактов? */
    if (!w->cfg->end_unlimited && w->tick >= w->cfg->end_max_ticks) {
        return RESULT_TICK_LIMIT;
    }

    return RESULT_RUNNING;
}

/* ─── Один такт ──────────────────────────────────────── */

void sim_tick(World *w) {
    w->tick++;

    /* phase 1: наблюдение */
    sensors_observe(w);

    /* phase 2: передача сообщений */
    messages_tick(w);

    /* phase 3: принятие решений центром    */
    center_tick(w);

    /* phase 4: перемещение групп           */
    teams_move(w);

    /* phase 5: тушение                      */
    teams_extinguish(w);

    /* phase 6: распространение огня */
    fire_spread(w);

    /* phase 7: публикация — render_frame */
}

/* ─── Печать результата ──────────────────────────────── */

const char *sim_result_name(SimResult r) {
    switch (r) {
        case RESULT_RUNNING:              return "RUNNING";
        case RESULT_EXTINGUISHED:         return "EXTINGUISHED";
        case RESULT_TERRITORY_EXHAUSTED:  return "TERRITORY_EXHAUSTED";
        case RESULT_TICK_LIMIT:           return "TICK_LIMIT";
        case RESULT_INTERRUPTED:          return "INTERRUPTED";
    }
    return "?";
}

SimResult sim_run(World *w) {
    SimResult r = sim_finished(w);
    while (r == RESULT_RUNNING) {
        sim_tick(w);
        r = sim_finished(w);
    }
    return r;
}