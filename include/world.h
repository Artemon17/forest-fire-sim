#ifndef WORLD_H
#define WORLD_H

#include "config.h"
#include "map.h"
#include "log.h"

/* Состояние пожара на клетке. */
typedef enum {
    ST_SAFE = 0,         /* не горело           */
    ST_BURNING,          /* горит               */
    ST_EXTINGUISHED,     /* потушено            */
    ST_BURNT             /* выгорело полностью  */
} FireState;

/* Что стоит на клетке. */
typedef enum {
    OCC_NONE = 0,
    OCC_SENSOR,
    OCC_STATION,
    OCC_TEAM
} Occupant;

typedef struct {
    int id;
    int r, c;
} Sensor;

typedef struct {
    int id;
    int r, c;
    /* target, state, progress — добавим в следующих ветках */
} FireTeam;


/* Одно сообщение от датчика. Уникальный id; дубликат несёт тот же id. */
typedef struct {
    int id;              /* уникальный номер сообщения      */
    int sensor_id;       /* какой датчик его создал          */
    int origin_r, origin_c; /* координаты обнаруженного очага */
    int created_tick;    /* такт создания                    */
    int delivery_tick;   /* такт, когда должно прибыть       */
    int is_duplicate;    /* 1 если это копия в канале        */
} Message;

typedef struct {
    Message *items;
    int      count;
    int      capacity;
} MessageQueue;

/*
 * World — состояние мира на текущий прогон.
 * MapData и Config — только для чтения, живут дольше World.
 */
typedef struct {
    const MapData *map;
    const Config  *cfg;

    int st_r, st_c;

    FireState *state;
    Occupant  *occupant;
    int       *team_id;
    int       *burn_age;

    Sensor   *sensors;
    int       sensors_count;

    FireTeam *teams;
    int       teams_count;

    EventLog  log;

    /* ── сообщения ── */
    int          next_message_id;
    MessageQueue in_flight;      /* сейчас в канале           */
    MessageQueue inbox;          /* доставлены в центр        */
    int         *seen_ids;       /* id, уже принятые центром  */
    int          seen_count;
    int          seen_cap;
    unsigned char *sensor_reported; /* [sensor_idx * n_cells + cell] */

    int tick;
} World;

/* Что показывать в клетке. */
typedef struct {
    char terrain;   /* '"', 't', 'w', '_' */
    char state;     /* ' ', 'F', '#' */
    char occupant;  /* ' ', '.', 'S', '1'..'9' */
} CellView;

int  world_init(World *w, const MapData *map, const Config *cfg,
                int st_r, int st_c);
void world_free(World *w);

CellView world_cell_view(const World *w, int r, int c);
char     world_display_symbol(const CellView *cv);

#endif