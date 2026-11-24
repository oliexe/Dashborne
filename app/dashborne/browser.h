#ifndef DASHBORNE_BROWSER_H
#define DASHBORNE_BROWSER_H
#define BROWSER_PATH 1024
#define BROWSER_MAX 256
struct browser_entry { char name[256]; int directory; };
void browser_init(void);
void browser_refresh(void);
void browser_up(void);
void browser_select(unsigned int index);
unsigned int browser_count(void);
const struct browser_entry *browser_entry(unsigned int index);
const char *browser_path(void);
const char *browser_selection(void);
const char *browser_status(void);
int browser_can_load(void);
int browser_prepare(void);
int browser_prepare_path(const char *path);
/* Consume before loading an app: a crash cannot repeatedly select it. */
int browser_take_request(char path[BROWSER_PATH]);
int browser_take_return(void);
void browser_cancel_request(void);
int browser_validate(const char *path);
#endif
