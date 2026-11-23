#ifndef SCREEN_MIRRORING_PRESENTATION_H
#define SCREEN_MIRRORING_PRESENTATION_H
#include "dashborne_extension.h"
int presentation_bind(const struct dashborne_host *, void *image_base);
int presentation_has_widget(void);
void presentation_update(int active);
void presentation_release(void);
void presentation_reset(void);
#endif
