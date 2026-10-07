#ifndef MESSAGES_H
#define MESSAGES_H

#include "world.h"

/* Датчик создаёт сообщение об очаге. Может потеряться, может дублироваться. */
void messages_send(World *w, int sensor_idx, int origin_r, int origin_c);

/* Фаза 2 такта: продвинуть in_flight, доставить дошедшие в inbox. */
void messages_tick(World *w);

void messages_send_team(World *w, int team_id, int r, int c);

#endif