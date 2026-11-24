#ifndef DASHBORNE_CONFIG_H
#define DASHBORNE_CONFIG_H
#define CONFIG_LIMIT 32768
#define CONFIG_VALUE 1024
/* One document per UI thread. Reload before a transaction, commit once. */
int config_load(void);
int config_get(const char *key, char *out, unsigned int capacity);
int config_set(const char *key, const char *value);
int config_commit(void);
void config_clear(void);
int config_wifi(char ssid[33], char password[64]);
#endif
