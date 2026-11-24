#ifndef DBW_RUNTIME_H
#define DBW_RUNTIME_H
#include "dashborne_widget.h"
typedef __SIZE_TYPE__ usize;
typedef __UINTPTR_TYPE__ uintptr;
#define PROVIDERS 32
#define FIRST_SOURCE 0x211u
#define LAST_SOURCE 0x2ffu
struct widget_provider {const struct dashborne_host *host;const struct dashborne_widget *widget;unsigned int source;};
struct widget_geometry {int columns,rows,type,width,height;};
struct cell {int x,y;};
struct layout {int type,x,y;unsigned short id,padding;unsigned short *begin,*end,*capacity;};
extern const struct widget_geometry widget_sizes[DBW_SIZE_COUNT];
extern struct widget_provider providers[PROVIDERS];
extern const struct dashborne_host *runtime_host;
const struct widget_provider *provider_for(unsigned int source);
int runtime_bind(void);
int runtime_unbind(void);
int runtime_live(void);
int registry_bind(const struct dashborne_host *);
int registry_cleanup(void);
int registry_publish(unsigned int,const struct dashborne_widget *);
unsigned int registry_source(const void *);
int views_bind(const struct dashborne_host *);
void views_tick(unsigned long long);
void views_pause(int);
int views_detach(void);
int views_cleanup(void);
void views_text(void *,const char *);
void views_invalidate(struct dashborne_widget_view *);
int views_click(void *);
void views_activity(void *);
#endif
