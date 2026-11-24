#ifndef TESLA_UI_SOURCE_H
#define TESLA_UI_SOURCE_H
#include "dashborne_extension.h"
#include "model.h"
int source_bind(const struct dashborne_host *);
void source_read(struct readings *);
unsigned int source_seconds(void);
int source_fingerprint(const char *,unsigned int,unsigned int);
#endif
