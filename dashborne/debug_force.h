#ifndef DASHBORNE_DEBUG_FORCE_H
#define DASHBORNE_DEBUG_FORCE_H
int debug_force_install(void (**change_mode)(int), void *network_init, void *maintenance);
int debug_force_remove(void (**change_mode)(int));
#endif
