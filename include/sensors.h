#ifndef SENSORS_H
#define SENSORS_H

#include "world.h"

/* Один такт наблюдения: каждый датчик сканирует свою зону. */
void sensors_observe(World *w);

/* Сколько горящих клеток видит датчик с индексом i. 0..N. */
int  sensor_burning_in_view(const World *w, int sensor_idx);

#endif