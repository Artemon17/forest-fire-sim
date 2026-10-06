#ifndef FIRE_H
#define FIRE_H

#include "world.h"

/* Один такт распространения огня. Изменяет w->state, w->burn_age, w->tick. */
void fire_spread(World *w);

/* Сколько тактов клетка горит до полного выгорания. */
int  fire_burn_ticks(const World *w, int r, int c);

#endif