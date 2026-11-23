#ifndef DASHBORNE_DEBUG_H
#define DASHBORNE_DEBUG_H
void debug_bind(void *stock, void *runtime, void (*logger)(const char *));
void debug_set_enabled(int enabled);
void debug_poll(void);
void debug_release(void);
const char *debug_status(void);
const char *debug_ip(void);
#endif
