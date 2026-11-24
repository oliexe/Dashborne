#ifndef VEHICLE_VIEWS_H
#define VEHICLE_VIEWS_H
#include "dashborne_extension.h"
int views_bind(const struct dashborne_host *,void (*draw)(void *));
int views_create(void *activity);
int views_sync(void);
void views_invalidate(void);
void views_label(const char *);
void views_preview(int);
int views_click(void *control);
int views_editing(void);
int views_detach(void);
int views_cleanup(void);
#endif
