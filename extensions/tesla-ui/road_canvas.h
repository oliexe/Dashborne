#ifndef TESLA_UI_ROAD_CANVAS_H
#define TESLA_UI_ROAD_CANVAS_H
#include "dashborne_extension.h"
int road_canvas_bind(const struct dashborne_host *);
int road_canvas_attach(void *control);
void road_canvas_detach(void);
void road_canvas_frame(float phase,unsigned long long now);
#endif
