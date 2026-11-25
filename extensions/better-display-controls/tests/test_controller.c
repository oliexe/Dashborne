#include <assert.h>
#include <stdio.h>
#include <string.h>
#define clock_gettime fake_clock_gettime
#define time fake_time
#define localtime_r fake_localtime
#define open fake_open
#define close fake_close
#define read fake_read
#define crc32 fake_crc32
#include "../controller.c"
#undef clock_gettime
#undef time
#undef localtime_r
#undef open
#undef close
#undef read
#undef crc32
static long epoch,ticks;static int clock_fail;
int fake_clock_gettime(int id,struct display_timespec *out){assert(id==1);out->sec=ticks;out->nsec=0;return clock_fail?-1:0;}
static int minute,apply_fail,restore_fail,save_fail,applied,restores,brightness,black;
long fake_time(long *p){(void)p;return epoch;}
struct native_tm *fake_localtime(const long *p,struct native_tm *out){(void)p;*out=(struct native_tm){.hour=minute/60,.min=minute%60};return out;}
int fake_open(const char *p,int f,...){(void)p;(void)f;return -1;}
int fake_close(int f){(void)f;return 0;}
int fake_read(int f,void *p,unsigned int n){(void)f;(void)p;(void)n;return -1;}
unsigned long fake_crc32(unsigned long c,const unsigned char *p,unsigned int n){(void)c;(void)p;(void)n;return 0;}
int display_hardware_bind(const struct dashborne_host *h){(void)h;return 0;}
int display_hardware_apply(int b,int l){applied++;brightness=b;black=l;return apply_fail?-1:0;}
int display_hardware_restore(void){restores++;return restore_fail?-1:0;}
void display_hardware_close(void){}
int display_settings_load(struct display_settings *s){display_defaults(s);return 1;}
int display_settings_save(const struct display_settings *s){assert(display_valid(s));return !save_fail;}
int memory_prepare(struct memory_edit *e,void *p,const void *a,const void *b,unsigned int n){(void)e;(void)p;(void)a;(void)b;(void)n;return 0;}
int memory_apply(struct memory_edit *e){(void)e;return 0;}
int memory_restore(struct memory_edit *e){(void)e;return 0;}
int main(void){
 display_defaults(&display_settings);display_settings.enabled=1;
 display_tick();assert(!applied&&display_active==-1&&display_minute==-1&&restores==1);
 display_settings.mode=2;display_changed();assert(applied==1&&brightness==16&&black==50&&display_active==1);
 display_settings.mode=0;epoch=1800000000L;minute=420;display_tick();assert(display_active==0&&brightness==160);
 minute=1200;display_tick();assert(display_active==1&&brightness==16);
 /* Clock reset relinquishes the override instead of guessing a daytime level. */
 epoch=0;int count=applied;display_tick();assert(display_active==-1&&applied==count);
 epoch=1800000000L;apply_fail=1;display_tick();assert(!display_settings.enabled&&display_dirty);
 display_tick();assert(strstr(display_status,"failed"));apply_fail=0;
 display_changed();assert(!strcmp(display_status,"Stock display controls"));
 save_fail=1;assert(!display_save());display_tick();assert(strstr(display_status,"save"));
 save_fail=0;assert(display_save()&&!display_dirty);assert(!strcmp(display_status,"Stock display controls"));
 /* Preview expiry uses monotonic time, restores the preceding profile and dirty state. */
 display_settings.mode=1;display_settings.enabled=0;ticks=100;
 assert(display_preview_begin());display_settings.enabled=1;display_settings.day.brightness=6;display_changed();
 assert(display_preview_seconds()==15&&brightness==6);
 ticks=114;epoch=0;display_tick();assert(display_settings.enabled);
 ticks=115;display_tick();assert(!display_settings.enabled&&display_settings.day.brightness==160&&!display_dirty);
 assert(strstr(display_status,"reverted"));
 assert(display_preview_begin());display_settings.enabled=1;display_changed();assert(display_save());
 ticks=200;display_tick();assert(display_settings.enabled&&!display_preview_seconds());
 assert(display_preview_begin());display_settings.day.brightness=255;display_changed();
 display_preview_cancel();assert(display_settings.day.brightness==160&&!display_dirty);
 clock_fail=1;assert(!display_preview_begin());clock_fail=0;
 assert(display_preview_begin());display_settings.day.brightness=6;display_changed();
 save_fail=1;assert(!display_save()&&display_preview_seconds());save_fail=0;
 ticks+=16;display_tick();assert(display_settings.day.brightness==160);
 restore_fail=1;assert(display_controller_stop()==-1);restore_fail=0;assert(!display_controller_stop());
 puts("display controller: unsynced clock, transitions, clock reset, sticky errors and cleanup retry passed");
}
