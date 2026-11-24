#ifndef TESLA_UI_SAMPLE_H
#define TESLA_UI_SAMPLE_H
#include "model.h"
#define SAMPLE_SCENES 19
#define SAMPLE_SCENE_US 6000000ull
/* Local renderer input only; never touches the stock telemetry buffers. */
const char *sample_read(struct readings *,unsigned long long elapsed_us);
#endif
