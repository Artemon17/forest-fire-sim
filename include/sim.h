#ifndef SIM_H
#define SIM_H

#include "world.h"

/* Как завершился прогон. */
typedef enum {
    RESULT_RUNNING = 0,       /* ещё идёт, не финал */
    RESULT_EXTINGUISHED,      /* пожар потушен */
    RESULT_OUT_OF_BOUNDS,     /* огонь вышел за границу контроля */
    RESULT_TERRITORY_EXHAUSTED,/* всё, что могло, выгорело */
    RESULT_TICK_LIMIT,        /* достигнут end.max_ticks */
    RESULT_INTERRUPTED        /* Ctrl+C */
} SimResult;

/* Один такт: все фазы в строгом порядке. */
void sim_tick(World *w);

/* Проверить условия завершения. */
SimResult sim_finished(const World *w);

/* Строковое имя результата. */
const char *sim_result_name(SimResult r);

#endif