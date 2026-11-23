#ifndef SCREEN_MIRRORING_AWAKE_H
#define SCREEN_MIRRORING_AWAKE_H
#include "dashborne_extension.h"
int awake_install(const struct dashborne_host *host, void *image_base);
int awake_restore(void);
#endif
