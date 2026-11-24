#ifndef TESLA_MODEL_DATA_H
#define TESLA_MODEL_DATA_H
#include "dashborne_extension.h"
#include "scene.h"
int tm_data_bind(const struct dashborne_host *);
void tm_data_read(struct tm_reading *);
void tm_decode_panels(struct tm_reading *,const volatile unsigned char *);
#endif
