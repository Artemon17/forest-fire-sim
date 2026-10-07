#define _POSIX_C_SOURCE 200809L

#include "teams.h"

#include <stdlib.h>
#include <string.h>

static const int DR[8] = { -1, -1,  0, +1, +1, +1,  0, -1 };
static const int DC[8] = {  0, +1, +1, +1,  0, -1, -1, -1 };

static int abs_i(int x) { return x < 0 ? -x : x; }

/* Можно ли группе стоять на клетке (r,c). */
static int team_can_stand(const World *w, int r, int c) {
    int R = w->map->rows, C = w->map->cols;
    if (r < 0 || r >= R || c < 0 || c >= C) return 0;

    int idx = r * C + c;
    Terrain t = w->map->cells[idx];

    if (t == TERRAIN_WATER && !w->cfg->team_can_cross_water) return 0;
    if (t == TERRAIN_FIREBREAK && !w->cfg->team_can_cross_firebreak) return 0;

    if (w->state[idx] == ST_BURNING) return 0;
    return 1;
}

/*
 * BFS от (sr,sc) до (tr,tc) с учётом препятствий.
 * Возвращает следующую клетку на пути через out_r/out_c.
 * Если уже на цели или путь не найден — возвращает 0.
 */
static int bfs_next_step(const World *w,
                         int sr, int sc, int tr, int tc,
                         int *out_r, int *out_c)
{
    int R = w->map->rows, C = w->map->cols;
    int n = R * C;
    if (sr == tr && sc == tc) return 0;

    int *prev = malloc((size_t)n * sizeof(int));
    int *queue = malloc((size_t)n * sizeof(int));
    if (!prev || !queue) { free(prev); free(queue); return 0; }

    for (int i = 0; i < n; i++) prev[i] = -1;

    int head = 0, tail = 0;
    int start = sr * C + sc;
    int goal  = tr * C + tc;

    queue[tail++] = start;
    prev[start] = start;

    int found = 0;
    while (head < tail) {
        int cur = queue[head++];
        if (cur == goal) { found = 1; break; }

        int cr = cur / C, cc = cur % C;
        for (int d = 0; d < 8; d++) {
            int nr = cr + DR[d];
            int nc = cc + DC[d];
            if (nr < 0 || nr >= R || nc < 0 || nc >= C) continue;
            int nidx = nr * C + nc;
            if (prev[nidx] != -1) continue;
            /* цель может быть горящей — это разрешено, тушим с соседней клетки */
            if (nidx == goal) {
                prev[nidx] = cur;
                queue[tail++] = nidx;
                continue;
            }
            if (!team_can_stand(w, nr, nc)) continue;
            /* не заходить на клетку, где уже стоит другая группа */
            if (w->occupant[nidx] == OCC_TEAM) continue;
            prev[nidx] = cur;
            queue[tail++] = nidx;
        }
    }

    if (!found) {
        free(prev); free(queue);
        return 0;
    }

    /* идём назад от goal до старта, берём клетку прямо перед start */
    int cur = goal;
    while (prev[cur] != start && prev[cur] != cur) {
        cur = prev[cur];
    }

    *out_r = cur / C;
    *out_c = cur % C;

    free(prev); free(queue);
    return 1;
}

/* ─── Позиция группы на карте ─────────────────────────── */

static void team_mark_leave(World *w, FireTeam *t) {
    int idx = t->r * w->map->cols + t->c;
    /* стираем, только если это была наша клетка */
    if (w->occupant[idx] == OCC_TEAM && w->team_id[idx] == t->id) {
        w->occupant[idx] = OCC_NONE;
        w->team_id[idx]  = 0;
    }
}

static void team_mark_enter(World *w, FireTeam *t) {
    int idx = t->r * w->map->cols + t->c;
    w->occupant[idx] = OCC_TEAM;
    w->team_id[idx]  = t->id;
}

/* ─── Найти задачу по id ──────────────────────────────── */

static Task *find_task(World *w, int id) {
    for (int i = 0; i < w->tasks.count; i++)
        if (w->tasks.items[i].id == id) return &w->tasks.items[i];
    return NULL;
}

/* ─── Фаза 4: движение ────────────────────────────────── */

void teams_move(World *w) {
    for (int i = 0; i < w->teams_count; i++) {
        FireTeam *t = &w->teams[i];
        if (t->state != TEAM_MOVING && t->state != TEAM_RETURNING) continue;

        int goal_r, goal_c;
        if (t->state == TEAM_RETURNING) {
            goal_r = w->st_r;
            goal_c = w->st_c;
        } else {
            goal_r = t->target_r;
            goal_c = t->target_c;
        }

        /* если уже на месте — переключим состояние в другой фазе */
        if (t->r == goal_r && t->c == goal_c) {
            if (t->state == TEAM_RETURNING) {
                t->state = TEAM_IDLE;
                log_add(&w->log, w->tick,
                        "Группа #%d вернулась на станцию", t->id);
            }
            continue;
        }

        /* если группа соседствует с целью (Чебышёв <= 1) и цель горит —
         * дальше не идём, начинаем тушить */
        if (t->state == TEAM_MOVING) {
            int adj = abs_i(t->r - goal_r) <= 1 && abs_i(t->c - goal_c) <= 1;
            int target_burning =
                w->state[goal_r * w->map->cols + goal_c] == ST_BURNING;
            if (adj && target_burning) continue;
        }

        int nr, nc;
        if (!bfs_next_step(w, t->r, t->c, goal_r, goal_c, &nr, &nc)) {
            /* путь не найден. если это возврат — телепорт-выход, иначе ждём */
            continue;
        }

        team_mark_leave(w, t);
        t->r = nr;
        t->c = nc;
        team_mark_enter(w, t);

        log_add(&w->log, w->tick,
                "Группа #%d: -> (%d,%d)", t->id, nr + 1, nc + 1);
    }
}

/* ─── Фаза 5: тушение ─────────────────────────────────── */

void teams_extinguish(World *w) {
    for (int i = 0; i < w->teams_count; i++) {
        FireTeam *t = &w->teams[i];
        if (t->state != TEAM_MOVING && t->state != TEAM_EXTINGUISHING) continue;

        Task *task = find_task(w, t->target_task_id);
        if (!task) continue;

        int idx = t->target_r * w->map->cols + t->target_c;

        /* цель перестала гореть */
        if (w->state[idx] != ST_BURNING) {
            if (w->state[idx] == ST_BURNT) {
                task->state = TASK_CANCELLED;
                log_add(&w->log, w->tick,
                        "Задача #%d отменена: (%d,%d) выгорело",
                        task->id, task->r + 1, task->c + 1);
            } else {
                task->state = TASK_DONE;
                log_add(&w->log, w->tick,
                        "Задача #%d закрыта: (%d,%d) потушено",
                        task->id, task->r + 1, task->c + 1);
            }
            task->completed_tick = w->tick;

            t->target_task_id = 0;
            t->target_r = -1;
            t->target_c = -1;
            t->progress = 0.0;

            if (w->cfg->team_return_to_station) {
                t->state = TEAM_RETURNING;
            } else {
                t->state = TEAM_IDLE;
            }
            continue;
        }

        /* группа ещё не дошла до цели */
        int adj = abs_i(t->r - t->target_r) <= 1 &&
                  abs_i(t->c - t->target_c) <= 1;
        if (!adj) continue;

        if (t->state == TEAM_MOVING) {
            t->state = TEAM_EXTINGUISHING;
            task->state = TASK_IN_PROGRESS;
            log_add(&w->log, w->tick,
                    "Группа #%d начала тушение задачи #%d (%d,%d)",
                    t->id, task->id, task->r + 1, task->c + 1);
        }

        t->progress += w->cfg->team_efficiency;

        log_add(&w->log, w->tick,
                "Группа #%d тушит (%d,%d): %.1f / %d",
                t->id, t->target_r + 1, t->target_c + 1,
                t->progress, w->cfg->team_extinguish_time);

        if (t->progress >= (double)w->cfg->team_extinguish_time) {
            w->state[idx] = ST_EXTINGUISHED;
            w->burn_age[idx] = -1;

            task->state = TASK_DONE;
            task->completed_tick = w->tick;

            log_add(&w->log, w->tick,
                    "Группа #%d потушила (%d,%d)",
                    t->id, t->target_r + 1, t->target_c + 1);

            t->target_task_id = 0;
            t->target_r = -1;
            t->target_c = -1;
            t->progress = 0.0;

            if (w->cfg->team_return_to_station) {
                t->state = TEAM_RETURNING;
            } else {
                t->state = TEAM_IDLE;
            }
        }
    }
}