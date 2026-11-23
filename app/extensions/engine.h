#ifndef DASHBORNE_EXTENSION_ENGINE_H
#define DASHBORNE_EXTENSION_ENGINE_H
int extensions_recover(void (*logger)(const char *));
int extension_probe(const char *id);
int extensions_start(const char *const *ids, unsigned int count, void *official, void *runtime,
                     const char *protected_target, void (*logger)(const char *));
const char *extensions_activity(const char *official_activity);
int extensions_stop(void);
#endif
