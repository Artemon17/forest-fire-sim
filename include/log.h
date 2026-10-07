#ifndef LOG_H
#define LOG_H

#define LOG_MAX_EVENTS  512    /* сколько событий хранится в кольцевом буфере */
#define LOG_MAX_TEXT    160    /* максимальная длина одной строки события    */

typedef struct {
    int  tick;                 /* такт, в котором событие произошло */
    char text[LOG_MAX_TEXT];   /* человекочитаемый текст события    */
} LogEvent;

typedef struct {
    LogEvent events[LOG_MAX_EVENTS];
    int      head;             /* индекс следующей записи (куда писать) */
    int      count;            /* всего записано с начала симуляции     */
} EventLog;

/* Обнулить лог. Вызывается при world_init. */
void log_init(EventLog *log);

/* Добавить событие. Работает как printf — формат + аргументы. */
void log_add(EventLog *log, int tick, const char *fmt, ...);

/*
 * Скопировать последние n событий в out[] в порядке от старого к новому.
 * Возвращает число реально скопированных (может быть меньше n, если событий
 * пока мало).
 *
 * out должен указывать на массив минимум из n элементов LogEvent.
 */
int  log_last_n(const EventLog *log, int n, LogEvent *out);

#endif