#ifndef DASHBORNE_DEBUG_H
#define DASHBORNE_DEBUG_H
void debug_bind(void *stock, void *runtime, void (*logger)(const char *));
void debug_set_enabled(int enabled);
int debug_set_forced(int enabled);
int debug_is_forced(void);
void debug_poll(void);
void debug_release(void);
const char *debug_status(void);
const char *debug_ip(void);
/* Configured developer network while ADB is enabled, not a connection claim. */
const char *debug_wifi_name(void);
#endif
