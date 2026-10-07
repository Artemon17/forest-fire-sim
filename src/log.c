#define _POSIX_C_SOURCE 200809L

#include "log.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

void log_init(EventLog *log) {
    memset(log, 0, sizeof(*log));
}

void log_add(EventLog *log, int tick, const char *fmt, ...) {
    LogEvent *e = &log->events[log->head];
    e->tick = tick;

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(e->text, LOG_MAX_TEXT, fmt, ap);
    va_end(ap);

    log->head = (log->head + 1) % LOG_MAX_EVENTS;
    log->count++;
}

int log_last_n(const EventLog *log, int n, LogEvent *out) {
    if (n <= 0) return 0;

    int have = log->count < LOG_MAX_EVENTS ? log->count : LOG_MAX_EVENTS;
    if (n > have) n = have;

    /* Последние n событий: они лежат в позициях
     *   (head - n), (head - n + 1), ..., (head - 1)
     * по модулю LOG_MAX_EVENTS.
     */
    int start = (log->head - n + LOG_MAX_EVENTS) % LOG_MAX_EVENTS;

    for (int i = 0; i < n; i++) {
        int idx = (start + i) % LOG_MAX_EVENTS;
        out[i] = log->events[idx];
    }
    return n;
}