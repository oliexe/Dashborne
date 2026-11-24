#ifndef TESLA_UI_VISUAL_H
#define TESLA_UI_VISUAL_H
#include "dashborne_extension.h"
int visual_bind(const struct dashborne_host *);
int visual_attach(int id,void *control);
void visual_detach(void);
int visual_text(void *control,const char *text);
int visual_vehicle(int drive,int mask,unsigned long long now,int immediate);
void visual_frame(unsigned long long now);
#endif
