#ifndef DASHBORNE_LAN_H
#define DASHBORNE_LAN_H
/* Called once per second. 1: fresh gateway reply, 0: pending, -1: failed.
   Never blocks or changes network configuration. */
int lan_poll(const char *ip);
void lan_reset(void);
const char *lan_error(void);
#endif
