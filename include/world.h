#ifndef WORLD_H
#define WORLD_H

#include "config.h"
#include "map.h"
#include "log.h"

/* ─── Состояние пожара на клетке ──────────────────────── */
typedef enum {
    ST_SAFE = 0,         /* не горело           */
    ST_BURNING,          /* горит               */
    ST_EXTINGUISHED,     /* потушено            */
    ST_BURNT             /* выгорело полностью  */
} FireState;

/* ─── Что стоит на клетке ─────────────────────────────── */
typedef enum {
    OCC_NONE = 0,
    OCC_SENSOR,
    OCC_STATION,
    OCC_TEAM
} Occupant;

/* ─── Датчик ──────────────────────────────────────────── */
typedef struct {
    int id;
    int r, c;
} Sensor;

/* ─── Состояние пожарной группы ───────────────────────── */
typedef enum {
    TEAM_IDLE = 0,
    TEAM_MOVING,
    TEAM_EXTINGUISHING,
    TEAM_RETURNING
} TeamState;

typedef struct {
    int       id;
    int       r, c;
    TeamState state;
    int       target_r, target_c;
    int       target_task_id;    /* 0 = свободна */
    double    progress;          /* 0 .. team_extinguish_time */
} FireTeam;

/* ─── Задача на очаг ──────────────────────────────────── */
typedef enum {
    TASK_PENDING = 0,   /* ждёт свободную группу */
    TASK_ASSIGNED,      /* назначена группе      */
    TASK_IN_PROGRESS,   /* группа тушит          */
    TASK_DONE,          /* потушено              */
    TASK_CANCELLED      /* отменена (выгорело)   */
} TaskState;

typedef struct {
    int       id;
    int       r, c;
    TaskState state;
    int       assigned_team_id;   /* 0 = не назначена */
    int       created_tick;
    int       completed_tick;
} Task;

typedef struct {
    Task *items;
    int   count;
    int   capacity;
} TaskList;

/* ─── Сообщение ───────────────────────────────────────── */
typedef struct {
    int id;                  /* уникальный номер              */
    int sensor_id;           /* какой датчик его создал       */
    int origin_r, origin_c;  /* координаты обнаруженного очага */
    int created_tick;
    int delivery_tick;       /* такт, когда должно прибыть    */
    int is_duplicate;        /* 1 если это копия в канале     */
} Message;

typedef struct {
    Message *items;
    int      count;
    int      capacity;
} MessageQueue;

/* ─── Мир ─────────────────────────────────────────────── */
typedef struct {
    const MapData *map;
    const Config  *cfg;

    int st_r, st_c;            /* позиция станции в этом прогоне */

    FireState *state;          /* rows*cols */
    Occupant  *occupant;       /* rows*cols */
    int       *team_id;        /* rows*cols, 0 = нет команды */
    int       *burn_age;       /* rows*cols, -1 = не горит, иначе возраст */

    Sensor   *sensors;
    int       sensors_count;

    FireTeam *teams;
    int       teams_count;

    /* ── сообщения ── */
    int          next_message_id;
    MessageQueue in_flight;      /* сейчас в канале         */
    MessageQueue inbox;          /* доставлены в центр      */
    int         *seen_ids;       /* id, уже принятые центром */
    int          seen_count;
    int          seen_cap;
    unsigned char *sensor_reported; /* [sensor_idx * n_cells + cell] */

    /* ── задачи и центр ── */
    TaskList tasks;
    int      next_task_id;

    /* ── журнал событий ── */
    EventLog log;

    int tick;
} World;

/* ─── Что показывать в клетке ─────────────────────────── */
typedef struct {
    char terrain;   /* '"', 't', 'w', '_' */
    char state;     /* ' ', 'F', '#'      */
    char occupant;  /* ' ', '.', 'S', '1'..'9' */
} CellView;

/* ─── API ─────────────────────────────────────────────── */

int  world_init(World *w, const MapData *map, const Config *cfg,
                int st_r, int st_c);
void world_free(World *w);

CellView world_cell_view(const World *w, int r, int c);
char     world_display_symbol(const CellView *cv);

#endif