#ifndef DASHBORNE_WIFI_DEBUG_H
#define DASHBORNE_WIFI_DEBUG_H
void wifi_debug_bind(void *stock, void (*logger)(const char *));
void wifi_debug_poll(void);
void wifi_debug_retry(void);
const char *wifi_debug_status(void);
const char *wifi_debug_ip(void);
#endif
