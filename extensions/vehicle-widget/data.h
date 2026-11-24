#ifndef VEHICLE_WIDGET_DATA_H
#define VEHICLE_WIDGET_DATA_H
#include "../tesla-ui/source.h"
void vehicle_read(struct readings *);
void vehicle_panel_read(struct readings *,const volatile unsigned char *);
#endif
