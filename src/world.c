#define _POSIX_C_SOURCE 200809L

#include "world.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int world_init(World *w, const MapData *map, const Config *cfg,
               int st_r, int st_c)
{
    memset(w, 0, sizeof(*w));
    w->map = map;
    w->cfg = cfg;
    w->st_r = st_r;
    w->st_c = st_c;

    int n = map->rows * map->cols;

    w->state    = calloc((size_t)n, sizeof(FireState));
    w->occupant = calloc((size_t)n, sizeof(Occupant));
    w->team_id  = calloc((size_t)n, sizeof(int));
    w->burn_age = malloc((size_t)n * sizeof(int));

    if (!w->state || !w->occupant || !w->team_id || !w->burn_age) {
        world_free(w);
        return -1;
    }

    for (int i = 0; i < n; i++) {
        w->state[i]    = ST_SAFE;
        w->occupant[i] = OCC_NONE;
        w->burn_age[i] = -1;
    }

    /* ── станция ── */
    w->occupant[st_r * map->cols + st_c] = OCC_STATION;

    /* ── датчики ── */
    w->sensors_count = map->sensors_count;
    if (w->sensors_count > 0) {
        w->sensors = calloc((size_t)w->sensors_count, sizeof(Sensor));
        if (!w->sensors) { world_free(w); return -1; }
        for (int i = 0; i < w->sensors_count; i++) {
            w->sensors[i].id = i + 1;
            w->sensors[i].r  = map->sensors[i].r;
            w->sensors[i].c  = map->sensors[i].c;
            w->occupant[w->sensors[i].r * map->cols + w->sensors[i].c] = OCC_SENSOR;
        }
    }

    /* ── группы ── */
    w->teams_count = cfg->team_count;
    if (w->teams_count > 0) {
        w->teams = calloc((size_t)w->teams_count, sizeof(FireTeam));
        if (!w->teams) { world_free(w); return -1; }
        for (int i = 0; i < w->teams_count; i++) {
            w->teams[i].id             = i + 1;
            w->teams[i].r              = st_r;
            w->teams[i].c              = st_c;
            w->teams[i].state          = TEAM_IDLE;
            w->teams[i].target_r       = -1;
            w->teams[i].target_c       = -1;
            w->teams[i].target_task_id = 0;
        }
    }

    /* ── начальные очаги ── */
    for (int i = 0; i < map->ignition_count; i++) {
        int r = map->ignition[i].r;
        int c = map->ignition[i].c;
        int idx = r * map->cols + c;
        w->state[idx]    = ST_BURNING;
        w->burn_age[idx] = 0;
    }

    /* ── сообщения ── */
    w->next_message_id = 1;
    w->sensor_reported = calloc((size_t)w->sensors_count *
                                (size_t)(map->rows * map->cols), 1);
    if (!w->sensor_reported && w->sensors_count > 0) {
        world_free(w);
        return -1;
    }

    /* ── задачи ── */
    w->next_task_id = 1;

    /* ── журнал ── */
    log_init(&w->log);

    w->tick = 0;
    return 0;
}

void world_free(World *w) {
    if (!w) return;
    free(w->state);
    free(w->occupant);
    free(w->team_id);
    free(w->burn_age);
    free(w->sensors);
    free(w->teams);
    free(w->in_flight.items);
    free(w->inbox.items);
    free(w->seen_ids);
    free(w->sensor_reported);
    free(w->tasks.items);
    memset(w, 0, sizeof(*w));
}

CellView world_cell_view(const World *w, int r, int c) {
    CellView cv;
    int idx = r * w->map->cols + c;

    switch (w->map->cells[idx]) {
        case TERRAIN_GRASS:     cv.terrain = '"'; break;
        case TERRAIN_TREE:      cv.terrain = 't'; break;
        case TERRAIN_WATER:     cv.terrain = 'w'; break;
        case TERRAIN_FIREBREAK: cv.terrain = '_'; break;
        default:                cv.terrain = '?'; break;
    }

    switch (w->state[idx]) {
        case ST_BURNING:      cv.state = 'F'; break;
        case ST_BURNT:        cv.state = '#'; break;
        case ST_EXTINGUISHED: cv.state = ' '; break;
        default:              cv.state = ' '; break;
    }

    switch (w->occupant[idx]) {
        case OCC_SENSOR:  cv.occupant = '.'; break;
        case OCC_STATION: cv.occupant = 'S'; break;
        case OCC_TEAM: {
            int tid = w->team_id[idx];
            cv.occupant = (tid >= 1 && tid <= 9) ? (char)('0' + tid) : 'T';
            break;
        }
        default:          cv.occupant = ' '; break;
    }

    return cv;
}

char world_display_symbol(const CellView *cv) {
    if (cv->occupant >= '1' && cv->occupant <= '9') return cv->occupant;
    if (cv->state == 'F')                          return 'F';
    if (cv->state == '#')                          return '#';
    if (cv->occupant == 'S')                       return 'S';
    if (cv->occupant == '.')                       return '.';
    if (cv->terrain  == 'w')                       return 'w';
    if (cv->terrain  == '_')                       return '_';
    if (cv->terrain  == 't')                       return 't';
    return '"';
}