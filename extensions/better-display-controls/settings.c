#include "model.h"
/* Reuse the root-card parser and atomic, unknown-key-preserving transaction.
   All calls are on the UI thread; no private settings file or worker writes. */
#include "../../dashborne/config.c"
extern int snprintf(char *,__SIZE_TYPE__,const char *,...);
static const char *keys[]={"display.enabled","display.mode","display.day_start","display.night_start",
    "display.day.brightness","display.day.black","display.night.brightness","display.night.black"};
static int number(const char *s,int *out) {
    int n=0;if(!*s)return 0;
    for(;*s;++s){if(*s<'0'||*s>'9'||n>10000)return 0;n=n*10+*s-'0';}*out=n;return 1;
}
int display_settings_load(struct display_settings *s) {
    display_defaults(s);if(!config_load())return 0;
    int *values[]={&s->enabled,&s->mode,&s->day_start,&s->night_start,&s->day.brightness,&s->day.black,&s->night.brightness,&s->night.black};
    int ok=1;char value[CONFIG_VALUE];
    for(unsigned int i=0;i<8;++i)if(config_get(keys[i],value,sizeof(value))&&!number(value,values[i]))ok=0;
    config_clear();if(!ok||!display_valid(s)){display_defaults(s);return 0;}return 1;
}
int display_settings_save(const struct display_settings *s) {
    if(!display_valid(s)||!config_load())return 0;
    const int values[]={s->enabled,s->mode,s->day_start,s->night_start,s->day.brightness,s->day.black,s->night.brightness,s->night.black};
    int ok=1;char value[32];
    for(unsigned int i=0;i<8;++i){snprintf(value,sizeof(value),"%d",values[i]);if(!config_set(keys[i],value))ok=0;}
    if(ok)ok=config_commit();config_clear();return ok;
}
