#ifndef WORLD_H
#define WORLD_H

#include "config.h"
#include "map.h"

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

/*
 * World — состояние мира на текущий прогон.
 * MapData и Config — только для чтения, живут дольше World.
 */
typedef struct {
    const MapData *map;
    const Config  *cfg;

    int st_r, st_c;            /* позиция станции в этом прогоне */

    FireState *state;          /* rows*cols */
    Occupant  *occupant;       /* rows*cols */
    int       *team_id;        /* rows*cols, 0 = нет команды */
    int       *burn_age;       /* rows*cols, -1 = не горит, иначе такт возгорания */

    Sensor   *sensors;
    int       sensors_count;

    FireTeam *teams;
    int       teams_count;

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