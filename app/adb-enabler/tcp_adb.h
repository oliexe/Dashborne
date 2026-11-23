#ifndef ADB_ENABLER_TCP_ADB_H
#define ADB_ENABLER_TCP_ADB_H
void tcp_adb_bind(void *runtime, void (*logger)(const char *));
void tcp_adb_poll(int network_ready);
void tcp_adb_stop(void);
const char *tcp_adb_status(void);
#endif
