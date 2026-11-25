#ifndef BETTER_DISPLAY_HARDWARE_H
#define BETTER_DISPLAY_HARDWARE_H
#include "dashborne_extension.h"
int display_hardware_bind(const struct dashborne_host *);
int display_hardware_apply(int brightness,int black);
int display_hardware_restore(void);
void display_hardware_close(void);
#endif
