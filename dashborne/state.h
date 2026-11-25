#ifndef DASHBORNE_STATE_H
#define DASHBORNE_STATE_H
#define MAX_PATCHES 64
struct patch_entry {
    char id[32], title[64], description[128];
    int enabled, compatible, native;
};
struct settings {
    int wifi_adb;
    unsigned int count;
    struct patch_entry patches[MAX_PATCHES];
    int config_ok, loader_autolaunch;
    char loader_app[1024];
    int adb_force, adb_auto_restart;
};
void state_restore(void);
const char *state_failed_id(void);
void state_load(struct settings *);
unsigned int state_rejected_count(void);
int state_save(const struct settings *);
int state_apply(const struct settings *, void (*logger)(const char *));
#endif
