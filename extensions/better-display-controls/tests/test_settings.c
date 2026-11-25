#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../model.h"
#include "../../../dashborne/config.h"
int main(void) {
 struct display_settings s;assert(display_settings_load(&s));assert(!s.enabled);
 assert(config_load());assert(config_set("unrelated.keep","hello = # world"));assert(config_commit());config_clear();
 s.enabled=1;s.mode=2;s.day.brightness=6;s.night.brightness=255;s.day_start=1380;s.night_start=360;s.night.black=30;
 assert(display_settings_save(&s));struct display_settings r;assert(display_settings_load(&r));assert(!memcmp(&s,&r,sizeof(s)));
 char value[64];assert(config_load());assert(config_get("unrelated.keep",value,sizeof(value)));assert(!strcmp(value,"hello = # world"));
 assert(config_set("display.day.brightness","1"));assert(config_set("display.night.brightness","5"));assert(config_commit());config_clear();
 assert(display_settings_load(&r)&&r.enabled&&r.mode==2&&r.day.brightness==6&&r.night.brightness==6&&r.night.black==30);
 assert(display_settings_save(&r));assert(config_load());assert(config_get("display.day.brightness",value,sizeof(value))&&!strcmp(value,"6"));
 assert(config_set("display.mode","garbage"));assert(config_commit());config_clear();
 assert(!display_settings_load(&r)&&!r.enabled&&r.mode==0);
 assert(config_load());assert(config_set("display.mode","2"));
 assert(config_set("display.day.brightness","1234567890123456789012345678901234567890"));assert(config_commit());config_clear();
 assert(!display_settings_load(&r)&&!r.enabled);
 assert(!display_settings_save(&(struct display_settings){0}));
 puts("display settings: atomic round trip, unrelated keys and corrupt settings passed");
}
