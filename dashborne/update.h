#ifndef DASHBORNE_UPDATE_H
#define DASHBORNE_UPDATE_H
struct update_release {char id[24],version[32],os[32],notes[401];int supported;};
void update_poll(void);
int update_start(const char *action,const char *id);
int update_busy(void);
const char *update_status(void);
unsigned int update_count(void);
const struct update_release *update_release(unsigned int index);
void update_confirm(void);
#endif
