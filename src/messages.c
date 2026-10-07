#define _POSIX_C_SOURCE 200809L

#include "messages.h"

#include <stdlib.h>
#include <string.h>

/* ─── Очередь ─────────────────────────────────────────── */

static void queue_push(MessageQueue *q, const Message *m) {
    if (q->count >= q->capacity) {
        int ncap = q->capacity ? q->capacity * 2 : 16;
        Message *ni = realloc(q->items, (size_t)ncap * sizeof(Message));
        if (!ni) return;
        q->items = ni;
        q->capacity = ncap;
    }
    q->items[q->count++] = *m;
}

static void queue_remove_at(MessageQueue *q, int idx) {
    if (idx < 0 || idx >= q->count) return;
    if (idx < q->count - 1)
        memmove(&q->items[idx], &q->items[idx + 1],
                (size_t)(q->count - idx - 1) * sizeof(Message));
    q->count--;
}

/* ─── Учёт обработанных id ────────────────────────────── */

static int seen_has(const World *w, int id) {
    for (int i = 0; i < w->seen_count; i++)
        if (w->seen_ids[i] == id) return 1;
    return 0;
}

static int seen_add(World *w, int id) {
    if (w->seen_count >= w->seen_cap) {
        int ncap = w->seen_cap ? w->seen_cap * 2 : 32;
        int *ni = realloc(w->seen_ids, (size_t)ncap * sizeof(int));
        if (!ni) return -1;
        w->seen_ids = ni;
        w->seen_cap = ncap;
    }
    w->seen_ids[w->seen_count++] = id;
    return 0;
}

/* ─── Случайное число [0,1) ───────────────────────────── */

static double rnd01(void) {
    return (double)rand() / ((double)RAND_MAX + 1.0);
}

static int rnd_range(int lo, int hi) {
    if (hi < lo) { int t = lo; lo = hi; hi = t; }
    return lo + rand() % (hi - lo + 1);
}

/* ─── Создание сообщения ──────────────────────────────── */

void messages_send(World *w, int sensor_idx, int origin_r, int origin_c) {
    const Config *cfg = w->cfg;
    const Sensor *s   = &w->sensors[sensor_idx];

    /* потеря на создании */
    if (rnd01() < cfg->msg_loss_prob) {
        log_add(&w->log, w->tick,
                "Сообщение от датчика #%d потеряно в канале",
                s->id);
        return;
    }

    int delay = rnd_range(cfg->msg_delay_min, cfg->msg_delay_max);

    Message m;
    m.id            = w->next_message_id++;
    m.sensor_id     = s->id;
    m.origin_r      = origin_r;
    m.origin_c      = origin_c;
    m.created_tick  = w->tick;
    m.delivery_tick = w->tick + delay;
    m.is_duplicate  = 0;

    queue_push(&w->in_flight, &m);

    log_add(&w->log, w->tick,
            "Датчик #%d шлёт сообщение #%d о (%d,%d), задержка %d",
            s->id, m.id, origin_r + 1, origin_c + 1, delay);

    /* дублирование */
    if (rnd01() < cfg->msg_dup_prob) {
        Message dup = m;
        dup.is_duplicate = 1;
        queue_push(&w->in_flight, &dup);
        log_add(&w->log, w->tick,
                "Сообщение #%d продублировано в канале", m.id);
    }
}

/* ─── Фаза 2: продвижение и доставка ──────────────────── */

void messages_tick(World *w) {
    for (int i = 0; i < w->in_flight.count; ) {
        Message *m = &w->in_flight.items[i];
        if (m->delivery_tick > w->tick) { i++; continue; }

        if (seen_has(w, m->id)) {
            log_add(&w->log, w->tick,
                    "Дубликат сообщения #%d отброшен", m->id);
        } else {
            seen_add(w, m->id);
            queue_push(&w->inbox, m);
            log_add(&w->log, w->tick,
                    "Сообщение #%d доставлено в центр: (%d,%d)",
                    m->id, m->origin_r + 1, m->origin_c + 1);
        }
        queue_remove_at(&w->in_flight, i);
    }
}