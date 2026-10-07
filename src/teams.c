#define _POSIX_C_SOURCE 200809L

#include "teams.h"
#include "messages.h"

#include <stdlib.h>
#include <string.h>

/* Радиус обзора группы для выбора следующей цели */
#define EXT_RADIUS 2

static const int DR[8] = { -1, -1,  0, +1, +1, +1,  0, -1 };
static const int DC[8] = {  0, +1, +1, +1,  0, -1, -1, -1 };

static int abs_i(int x) { return x < 0 ? -x : x; }

/* ─── Препятствия для групп ───────────────────────────── */

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

/* ─── BFS: следующий шаг ──────────────────────────────── */

static int bfs_next_step(const World *w,
                         int sr, int sc, int tr, int tc,
                         int *out_r, int *out_c)
{
    int R = w->map->rows, C = w->map->cols;
    int n = R * C;
    if (sr == tr && sc == tc) return 0;

    int *prev  = malloc((size_t)n * sizeof(int));
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

            if (nidx == goal) {
                prev[nidx] = cur;
                queue[tail++] = nidx;
                continue;
            }
            if (!team_can_stand(w, nr, nc)) continue;
            if (w->occupant[nidx] == OCC_TEAM) continue;

            prev[nidx] = cur;
            queue[tail++] = nidx;
        }
    }

    if (!found) { free(prev); free(queue); return 0; }

    int cur = goal;
    while (prev[cur] != start && prev[cur] != cur) cur = prev[cur];

    *out_r = cur / C;
    *out_c = cur % C;

    free(prev);
    free(queue);
    return 1;
}

/* ─── Метки на карте ──────────────────────────────────── */

static void team_mark_leave(World *w, FireTeam *t) {
    int idx = t->r * w->map->cols + t->c;
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

/* ─── Поиск задачи по id ──────────────────────────────── */

static Task *find_task(World *w, int id) {
    for (int i = 0; i < w->tasks.count; i++)
        if (w->tasks.items[i].id == id) return &w->tasks.items[i];
    return NULL;
}

/* ─── Найти ближайшую горящую клетку в радиусе ────────── */

static int find_nearby_fire(const World *w, int r, int c,
                            int *out_r, int *out_c)
{
    int R = w->map->rows, C = w->map->cols;
    int best_d = 1 << 30;
    int found = 0;

    for (int dr = -EXT_RADIUS; dr <= EXT_RADIUS; dr++) {
        for (int dc = -EXT_RADIUS; dc <= EXT_RADIUS; dc++) {
            if (dr == 0 && dc == 0) continue;
            int nr = r + dr, nc = c + dc;
            if (nr < 0 || nr >= R || nc < 0 || nc >= C) continue;
            if (w->state[nr * C + nc] != ST_BURNING) continue;

            int d = abs_i(dr) + abs_i(dc);
            if (d < best_d) {
                best_d = d;
                *out_r = nr;
                *out_c = nc;
                found = 1;
            }
        }
    }
    return found;
}

/* Добавить задачу в список */
static void push_task(World *w, const Task *t) {
    if (w->tasks.count >= w->tasks.capacity) {
        int ncap = w->tasks.capacity ? w->tasks.capacity * 2 : 8;
        Task *ni = realloc(w->tasks.items, (size_t)ncap * sizeof(Task));
        if (!ni) return;
        w->tasks.items = ni;
        w->tasks.capacity = ncap;
    }
    w->tasks.items[w->tasks.count++] = *t;
}

/* ─── Фаза 4: движение ────────────────────────────────── */

void teams_move(World *w) {
    for (int i = 0; i < w->teams_count; i++) {
        FireTeam *t = &w->teams[i];
        if (t->state != TEAM_MOVING && t->state != TEAM_RETURNING) continue;

        /* Если тушим — не двигаемся */
        if (t->state == TEAM_EXTINGUISHING) continue;

        int goal_r, goal_c;
        if (t->state == TEAM_RETURNING) {
            goal_r = w->st_r;
            goal_c = w->st_c;
        } else {
            goal_r = t->target_r;
            goal_c = t->target_c;
        }

        /* Если едем к очагу, а он уже не горит — закрываем задачу */
        if (t->state == TEAM_MOVING) {
            int tidx = goal_r * w->map->cols + goal_c;
            if (w->state[tidx] != ST_BURNING) {
                Task *task = find_task(w, t->target_task_id);
                if (task) {
                    task->state = (w->state[tidx] == ST_BURNT)
                                  ? TASK_CANCELLED : TASK_DONE;
                    task->completed_tick = w->tick;
                    log_add(&w->log, w->tick,
                            "Задача #%d закрыта: (%d,%d), группа #%d разворачивается",
                            task->id, task->r + 1, task->c + 1, t->id);
                }
                t->target_task_id = 0;
                t->target_r = -1;
                t->target_c = -1;
                t->progress = 0.0;

                /* Ищем новую цель рядом */
                int nr, nc;
                if (find_nearby_fire(w, t->r, t->c, &nr, &nc)) {
                    Task nt;
                    nt.id               = w->next_task_id++;
                    nt.r                = nr;
                    nt.c                = nc;
                    nt.state            = TASK_IN_PROGRESS;
                    nt.assigned_team_id = t->id;
                    nt.created_tick     = w->tick;
                    nt.completed_tick   = 0;
                    push_task(w, &nt);
                    t->target_task_id = nt.id;
                    t->target_r = nr;
                    t->target_c = nc;
                    t->state = TEAM_MOVING;
                    log_add(&w->log, w->tick,
                            "Группа #%d берёт новую цель (%d,%d)",
                            t->id, nr + 1, nc + 1);
                } else {
                    if (w->cfg->team_return_to_station) t->state = TEAM_RETURNING;
                    else                                t->state = TEAM_IDLE;
                }
                continue;
            }
        }

        /* Если уже на месте */
        if (t->r == goal_r && t->c == goal_c) {
            if (t->state == TEAM_RETURNING) {
                t->state = TEAM_IDLE;
                log_add(&w->log, w->tick,
                        "Группа #%d вернулась на станцию", t->id);
            }
            continue;
        }

        /* Если рядом с горящей целью — не идём, тушим */
        if (t->state == TEAM_MOVING) {
            int adj = abs_i(t->r - goal_r) <= 1 && abs_i(t->c - goal_c) <= 1;
            int target_burning =
                w->state[goal_r * w->map->cols + goal_c] == ST_BURNING;
            if (adj && target_burning) continue;
        }

        int steps = w->cfg->team_speed;
        if (steps < 1) steps = 1;

        for (int s = 0; s < steps; s++) {
            if (t->r == goal_r && t->c == goal_c) break;
            int adj = abs_i(t->r - goal_r) <= 1 && abs_i(t->c - goal_c) <= 1;
            int target_burning =
                w->state[goal_r * w->map->cols + goal_c] == ST_BURNING;
            if (t->state == TEAM_MOVING && adj && target_burning) break;

            int nr, nc;
            if (!bfs_next_step(w, t->r, t->c, goal_r, goal_c, &nr, &nc)) break;

            team_mark_leave(w, t);
            t->r = nr;
            t->c = nc;
            team_mark_enter(w, t);

            log_add(&w->log, w->tick,
                    "Группа #%d: -> (%d,%d)", t->id, t->r + 1, t->c + 1);
        }
    }
}

/* ─── Фаза 5: тушение ─────────────────────────────────── */

void teams_extinguish(World *w) {
    for (int i = 0; i < w->teams_count; i++) {
        FireTeam *t = &w->teams[i];
        if (t->state != TEAM_MOVING && t->state != TEAM_EXTINGUISHING) continue;

        Task *task = find_task(w, t->target_task_id);

        /* Если цель уже не горит — закрываем задачу */
        if (task) {
            int idx = t->target_r * w->map->cols + t->target_c;
            if (w->state[idx] != ST_BURNING) {
                task->state = (w->state[idx] == ST_BURNT)
                              ? TASK_CANCELLED : TASK_DONE;
                task->completed_tick = w->tick;
                t->target_task_id = 0;
                t->target_r = -1;
                t->target_c = -1;
                t->progress = 0.0;
                task = NULL;

                int nr, nc;
                if (find_nearby_fire(w, t->r, t->c, &nr, &nc)) {
                    Task nt;
                    nt.id               = w->next_task_id++;
                    nt.r                = nr;
                    nt.c                = nc;
                    nt.state            = TASK_IN_PROGRESS;
                    nt.assigned_team_id = t->id;
                    nt.created_tick     = w->tick;
                    nt.completed_tick   = 0;
                    push_task(w, &nt);

                    t->target_task_id = nt.id;
                    t->target_r = nr;
                    t->target_c = nc;
                    t->state = TEAM_MOVING;
                    log_add(&w->log, w->tick,
                            "Группа #%d берёт следующую цель (%d,%d)",
                            t->id, nr + 1, nc + 1);
                } else {
                    if (w->cfg->team_return_to_station) t->state = TEAM_RETURNING;
                    else                                t->state = TEAM_IDLE;
                    log_add(&w->log, w->tick,
                            "Группа #%d закончила работу в (%d,%d)",
                            t->id, t->r + 1, t->c + 1);
                }
                continue;
            }
        }

        if (!task) continue;

        int idx = t->target_r * w->map->cols + t->target_c;

        /* Ещё не дошли до цели */
        int adj = abs_i(t->r - t->target_r) <= 1 &&
                  abs_i(t->c - t->target_c) <= 1;
        if (!adj) continue;

        if (t->state == TEAM_MOVING) {
            t->state = TEAM_EXTINGUISHING;
            task->state = TASK_IN_PROGRESS;
            log_add(&w->log, w->tick,
                    "Группа #%d тушит (%d,%d)",
                    t->id, t->target_r + 1, t->target_c + 1);
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

            /* Ищем следующую цель в радиусе */
            int nr, nc;
            if (find_nearby_fire(w, t->r, t->c, &nr, &nc)) {
                Task nt;
                nt.id               = w->next_task_id++;
                nt.r                = nr;
                nt.c                = nc;
                nt.state            = TASK_IN_PROGRESS;
                nt.assigned_team_id = t->id;
                nt.created_tick     = w->tick;
                nt.completed_tick   = 0;
                push_task(w, &nt);

                t->target_task_id = nt.id;
                t->target_r = nr;
                t->target_c = nc;
                t->state = TEAM_MOVING;
                log_add(&w->log, w->tick,
                        "Группа #%d берёт следующую цель (%d,%d)",
                        t->id, nr + 1, nc + 1);
            } else {
                if (w->cfg->team_return_to_station) t->state = TEAM_RETURNING;
                else                                t->state = TEAM_IDLE;
            }
        }
    }
}

/* Команды как датчики: смотрят на 2 клетки вокруг себя */
void teams_observe(World *w) {
    int R = w->map->rows, C = w->map->cols;
    const int RAD = 2;

    for (int i = 0; i < w->teams_count; i++) {
        FireTeam *t = &w->teams[i];
        int reported = 0;

        for (int dr = -RAD; dr <= RAD && !reported; dr++) {
            for (int dc = -RAD; dc <= RAD; dc++) {
                if (dr == 0 && dc == 0) continue;
                int nr = t->r + dr;
                int nc = t->c + dc;
                if (nr < 0 || nr >= R || nc < 0 || nc >= C) continue;
                if (w->state[nr * C + nc] != ST_BURNING) continue;

                /* уже сообщали об этой клетке — не спамим */
                if (t->last_report_r == nr && t->last_report_c == nc) continue;

                messages_send_team(w, t->id, nr, nc);
                t->last_report_r = nr;
                t->last_report_c = nc;
                reported = 1;
                break;
            }
        }
    }
}