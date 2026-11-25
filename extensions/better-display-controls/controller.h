#ifndef BETTER_DISPLAY_CONTROLLER_H
#define BETTER_DISPLAY_CONTROLLER_H
#include "model.h"
#include "dashborne_extension.h"
extern struct display_settings display_settings;
extern int display_minute,display_active,display_dirty;
extern const char *display_status;
void display_tick(void);
void display_changed(void);
int display_save(void);
int display_controller_start(const struct dashborne_host *);
int display_controller_stop(void);
int display_window_bind(const struct dashborne_host *);
void display_window_open(void);
int display_window_cleanup(void);
#endif
