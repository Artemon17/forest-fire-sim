#define _POSIX_C_SOURCE 200809L

#include "center.h"

#include <stdlib.h>
#include <string.h>

/* ─── Работа со списком задач ─────────────────────────── */

static void tasks_push(TaskList *tl, const Task *t) {
    if (tl->count >= tl->capacity) {
        int ncap = tl->capacity ? tl->capacity * 2 : 8;
        Task *ni = realloc(tl->items, (size_t)ncap * sizeof(Task));
        if (!ni) return;
        tl->items = ni;
        tl->capacity = ncap;
    }
    tl->items[tl->count++] = *t;
}

/* Уже есть задача на этот очаг, которая ещё не закрыта? */
static int has_active_task(const World *w, int r, int c) {
    for (int i = 0; i < w->tasks.count; i++) {
        const Task *t = &w->tasks.items[i];
        if (t->r != r || t->c != c) continue;
        if (t->state == TASK_PENDING ||
            t->state == TASK_ASSIGNED ||
            t->state == TASK_IN_PROGRESS)
            return 1;
    }
    return 0;
}

static int abs_i(int x) { return x < 0 ? -x : x; }

/* ─── Принятие решений ────────────────────────────────── */

static void center_consume_inbox(World *w) {
    for (int i = 0; i < w->inbox.count; i++) {
        Message *m = &w->inbox.items[i];
        int r = m->origin_r;
        int c = m->origin_c;

        /* очаг уже закрыт? */
        if (w->state[r * w->map->cols + c] != ST_BURNING) {
            log_add(&w->log, w->tick,
                    "Центр: (%d,%d) уже не горит, сообщение #%d отброшено",
                    r + 1, c + 1, m->id);
            continue;
        }

        /* задача на этот очаг уже есть? */
        if (has_active_task(w, r, c)) {
            log_add(&w->log, w->tick,
                    "Центр: (%d,%d) уже в работе, сообщение #%d отброшено",
                    r + 1, c + 1, m->id);
            continue;
        }

        Task t;
        t.id              = w->next_task_id++;
        t.r               = r;
        t.c               = c;
        t.state           = TASK_PENDING;
        t.assigned_team_id = 0;
        t.created_tick    = w->tick;
        t.completed_tick  = 0;
        tasks_push(&w->tasks, &t);

        log_add(&w->log, w->tick,
                "Центр: создана задача #%d на очаг (%d,%d)",
                t.id, r + 1, c + 1);
    }

    /* inbox очищен — сообщения отработаны */
    w->inbox.count = 0;
}




static void center_assign_teams(World *w) {
    for (int i = 0; i < w->tasks.count; i++) {
        Task *t = &w->tasks.items[i];
        if (t->state != TASK_PENDING) continue;

        /* Если очаг уже не горит — задача отменена, не тратим группу */
        int idx = t->r * w->map->cols + t->c;
        if (w->state[idx] != ST_BURNING) {
            t->state = TASK_CANCELLED;
            t->completed_tick = w->tick;
            log_add(&w->log, w->tick,
                    "Центр: задача #%d отменена до назначения — (%d,%d) уже не горит",
                    t->id, t->r + 1, t->c + 1);
            continue;
        }

        /* ближайшая свободная группа (Манхэттен) */
        int best_j = -1;
        int best_d = 1 << 30;
        for (int j = 0; j < w->teams_count; j++) {
            FireTeam *tm = &w->teams[j];
            if (tm->target_task_id != 0) continue;
            int d = abs_i(tm->r - t->r) + abs_i(tm->c - t->c);
            if (d < best_d) { best_d = d; best_j = j; }
        }

        if (best_j < 0) continue;

        FireTeam *tm = &w->teams[best_j];
        tm->target_task_id = t->id;
        tm->target_r       = t->r;
        tm->target_c       = t->c;
        tm->state          = TEAM_MOVING;

        t->state            = TASK_ASSIGNED;
        t->assigned_team_id = tm->id;

        log_add(&w->log, w->tick,
                "Центр: задача #%d -> группа #%d (расстояние %d)",
                t->id, tm->id, best_d);
    }
}


void center_tick(World *w) {
    center_consume_inbox(w);
    center_assign_teams(w);
}