#ifndef DASHBORNE_WATCHDOG_H
#define DASHBORNE_WATCHDOG_H
/* One recovery per launch; a recovered process stays in configuration. */
int watchdog_start(void);
int watchdog_ready(void);
void watchdog_beat(void);
void watchdog_stop(void);
int watchdog_recovery(void);
#endif
