#ifndef TEAMS_H
#define TEAMS_H

#include "world.h"

/* Фаза 4: движение групп к цели.
 * Фаза 5: тушение цели (если группа на ней или рядом). */
void teams_move(World *w);
void teams_extinguish(World *w);
void teams_observe(World *w);

#endif