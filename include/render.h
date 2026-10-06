#ifndef RENDER_H
#define RENDER_H

#include "world.h"

/* Отрисовать кадр: очистить, шапка, карта, разделитель, лог. */
void render_frame(const World *w);

#endif