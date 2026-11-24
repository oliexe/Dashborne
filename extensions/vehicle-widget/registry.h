#ifndef VEHICLE_REGISTRY_H
#define VEHICLE_REGISTRY_H
#include "dashborne_extension.h"
#define VEHICLE_SOURCE 0x211u
/* Registration and layout persistence are installed before dashboard creation. */
int registry_bind(const struct dashborne_host *);
int registry_cleanup(void);
int registry_widget(const void *);
#endif
