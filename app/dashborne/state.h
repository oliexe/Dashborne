#ifndef DASHBORNE_STATE_H
#define DASHBORNE_STATE_H
#define MAX_PATCHES 16
struct patch_entry {
    char id[32], title[64], description[128];
    int enabled, compatible;
};
struct settings {
    int wifi_adb;
    unsigned int count;
    struct patch_entry patches[MAX_PATCHES];
};
void state_load(struct settings *);
int state_save(const struct settings *);
int state_apply(const struct settings *, void (*logger)(const char *));
#endif
