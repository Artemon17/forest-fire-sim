#define _POSIX_C_SOURCE 200809L

#include "render.h"
#include "term.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define FRAME_BUF (1 << 16)

static const char *color_for(char sym) {
    switch (sym) {
        case 'F': return C_FIRE;
        case '#': return C_BURNT;
        case 'w': return C_WATER;
        case '_': return C_FIREBRK;
        case '.': return C_SENSOR;
        case 'S': return C_STATION;
        case 't': return C_FOREST;
        case '"': return C_GRASS;
        default:
            if (sym >= '1' && sym <= '9') return C_TEAM;
            return C_RESET;
    }
}

void render_frame(const World *w) {
    /* Сколько строк было в прошлом кадре — чтобы вернуться в начало. */
    static int prev_lines = 0;
    static char buf[FRAME_BUF];

    size_t p = 0;

    /* ── Сколько строк лога поместится ──
     * Считаем бюджет под высоту терминала, чтобы кадр не скроллил экран. */
    int trows = 24, tcols = 80;
    if (term_size(&trows, &tcols) != 0) {
        trows = 40;  /* не терминал — берём с запасом */
    }
    int overhead = 6;                          /* шапка, 2 разделителя, «События:», запас */
    int available = trows - overhead - w->map->rows;
    if (available < 0)   available = 0;
    if (available > 8)   available = 8;        /* не больше восьми в любом случае */

    /* ── Вернуться в начало предыдущего кадра ── */
    if (prev_lines > 0) {
        char mv[32];
        int n = snprintf(mv, sizeof(mv), "\033[%dA\r", prev_lines);
        if (n > 0) {
            ssize_t wr = write(STDOUT_FILENO, mv, (size_t)n);
            (void)wr;
        }
    }

    int lines = 0;

    #define APPEND(...) do { \
        int _n = snprintf(buf + p, sizeof(buf) - p, __VA_ARGS__); \
        if (_n > 0) p += (size_t)_n; \
        if (p >= sizeof(buf)) p = sizeof(buf) - 1; \
    } while (0)

    #define ENDL() do { \
        APPEND("\033[K\r\n"); \
        lines++; \
    } while (0)

    /* ── Подсчёт ── */
    int burning = 0, burnt = 0, ext = 0;
    int total_cells = w->map->rows * w->map->cols;
    for (int i = 0; i < total_cells; i++) {
        if      (w->state[i] == ST_BURNING)      burning++;
        else if (w->state[i] == ST_BURNT)        burnt++;
        else if (w->state[i] == ST_EXTINGUISHED) ext++;
    }

    /* ── Шапка ── */
    APPEND("Такт %d | Ветер %s %d | Огонь: %d | Потухло: %d | Выгорело: %d | В пути: %d | В инбоксе: %d",
           w->tick,
           wind_dir_name(w->cfg->wind_direction),
           w->cfg->wind_power,
           burning, ext, burnt,
           w->in_flight.count,
           w->inbox.count);
    ENDL();

    int pend = 0, in_prog = 0, done = 0, cancelled = 0;
    for (int i = 0; i < w->tasks.count; i++) {
        switch (w->tasks.items[i].state) {
            case TASK_PENDING:     pend++;      break;
            case TASK_ASSIGNED:
            case TASK_IN_PROGRESS: in_prog++;   break;
            case TASK_DONE:        done++;      break;
            case TASK_CANCELLED:   cancelled++; break;
        }
    }
    int busy = 0;
    for (int i = 0; i < w->teams_count; i++)
        if (w->teams[i].target_task_id != 0) busy++;

    APPEND("Задачи: ждут %d, в работе %d, готово %d, отменено %d | Группы: занято %d из %d",
           pend, in_prog, done, cancelled, busy, w->teams_count);
    ENDL();
    
    APPEND("──────────────────────────────────────────────────────────");
    ENDL();

    /* ── Заголовок столбцов ── */
    APPEND("    ");
    for (int c = 1; c <= w->map->cols; c++)
        APPEND("%2d ", c);
    ENDL();

    /* ── Карта ── */
    for (int r = 0; r < w->map->rows; r++) {
        APPEND("%2d |", r + 1);
        for (int c = 0; c < w->map->cols; c++) {
            CellView cv = world_cell_view(w, r, c);
            char sym = world_display_symbol(&cv);
            const char *col = color_for(sym);
            APPEND("%s%c%s  ", col, sym, C_RESET);
        }
        ENDL();
    }

    APPEND("──────────────────────────────────────────────────────────");
    ENDL();

    /* ── События ── */
    APPEND("События:");
    ENDL();

    LogEvent last[8];
    int log_n = log_last_n(&w->log, available, last);
    if (log_n == 0) {
        APPEND("  (пока тихо)");
        ENDL();
    } else {
        for (int i = 0; i < log_n; i++) {
            APPEND("  [%d] %s", last[i].tick, last[i].text);
            ENDL();
        }
    }

    /* Стереть всё, что осталось НИЖЕ последней строки.
     * Если прошлый кадр был длиннее, его хвост сейчас ниже курсора. */
    APPEND("\033[J");

    #undef APPEND
    #undef ENDL

    /* Запомнить, сколько строк нарисовали — в следующий раз подняться на столько. */
    prev_lines = lines;

    /* ── Один write ── */
    ssize_t off = 0;
    while (off < (ssize_t)p) {
        ssize_t k = write(STDOUT_FILENO, buf + off, (size_t)p - (size_t)off);
        if (k < 0) break;
        off += k;
    }
}