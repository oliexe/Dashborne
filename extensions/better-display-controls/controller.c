#include "controller.h"
#include "hardware.h"
#include "stock_compat.h"
#include "../../dashborne/core/widget-runtime/memory.h"
extern long time(long *);
struct native_tm {int sec,min,hour,mday,mon,year,wday,yday,isdst;long gmtoff;const char *zone;};
extern struct native_tm *localtime_r(const long *,struct native_tm *);
struct display_settings display_settings;
int display_minute=-1,display_active=-1,display_dirty;
const char *display_status="Stock display controls";
static const struct dashborne_host *host;
static const char *error;
static void *dashboard;
static struct memory_edit hooks[5];
static void (*old_create)(void *),(*old_destroy)(void *),(*old_delete)(void *);
static int (*old_timer)(void *,int),(*old_secondary)(void *,int);
static void (*add_timer)(void *,int,int),(*remove_timer)(void *,int);
#define TIMER 97520
void display_tick(void) {
    long now=time(0);struct native_tm t;display_minute=-1;
    if(now>=1577836800L && localtime_r(&now,&t) && t.hour>=0&&t.hour<24&&t.min>=0&&t.min<60)
        display_minute=t.hour*60+t.min;
    int profile=display_profile_at(&display_settings,display_minute);
    if(!display_settings.enabled || profile<0) {
        if(display_hardware_restore()) {display_status="Could not restore display. Tap Restore stock to retry.";return;}
        display_active=-1;
        display_status=error?error:display_settings.enabled?"Waiting for the DASH clock. Choose Day or Night to use manually.":"Stock display controls";
        return;
    }
    struct display_profile p=profile?display_settings.night:display_settings.day;
    if(display_hardware_apply(p.brightness,p.black)) {
        display_settings.enabled=0;display_dirty=1;display_active=-1;
        error=display_status="Display adjustment failed; stock restore requested.";
        if(host)host->log(host,"better-display-controls: readback failed; adjustments disabled");return;
    }
    if(display_active!=profile && host)host->log(host,profile?"better-display-controls: night profile":"better-display-controls: day profile");
    display_active=profile;display_status=error?error:profile?"Night profile active":"Day profile active";
}
void display_changed(void) {error=0;display_dirty=1;display_tick();}
int display_save(void) {
    if(!display_settings_save(&display_settings)){error=display_status="Could not save to microSD. Retry Save.";return 0;}
    display_dirty=0;error=0;display_tick();return 1;
}
static void created(void *s) {old_create(s);dashboard=s;display_tick();add_timer(s,TIMER,1000);}
static int timer(void *s,int id) {if(id==TIMER){display_tick();return 1;}return old_timer(s,id);}
static int secondary(void *s,int id) {if(id==TIMER){display_tick();return 1;}return old_secondary(s,id);}
static void destroying(void *s) {remove_timer(s,TIMER);if(dashboard==s)dashboard=0;}
static void destroy(void *s) {destroying(s);old_destroy(s);}
static void delete_object(void *s) {destroying(s);old_delete(s);}
int display_controller_start(const struct dashborne_host *h) {
    host=h;if(db_stock_version()!=6114)return -1;
    if(display_hardware_bind(h))return -1;
    if(!display_settings_load(&display_settings))h->log(h,"better-display-controls: using disabled defaults; config unavailable or invalid");
    add_timer=h->resolve(h->ui_runtime,"_ZN7BaseApp13registerTimerEii");
    remove_timer=h->resolve(h->ui_runtime,"_ZN7BaseApp15unregisterTimerEi");
    void **v=h->resolve(h->official_library,"_ZTV16dash1actActivity");
    if(!v||!add_timer||!remove_timer)return -1;
    const int slots[]={6,7,24,2,3};
    const char *names[]={"_ZN16dash1actActivity8onCreateEv","_ZN16dash1actActivity7onTimerEi",
        "_ZThn4_N16dash1actActivity7onTimerEi","_ZN16dash1actActivityD1Ev","_ZN16dash1actActivityD0Ev"};
    void *replacements[]={(void *)created,(void *)timer,(void *)secondary,(void *)destroy,(void *)delete_object};
    for(int i=0;i<5;i++) {
        void *stock=h->resolve(h->official_library,names[i]),*original=v[slots[i]];
        if(!stock || (original!=stock && !h->owns_callback(h,original)))return -1;
        if(memory_prepare(&hooks[i],v+slots[i],&original,&replacements[i],sizeof(void *)))return -1;
    }
    old_create=v[6];old_timer=v[7];old_secondary=v[24];old_destroy=v[2];old_delete=v[3];
    for(int i=0;i<5;i++)if(memory_apply(&hooks[i]))return -1;
    return 0;
}
int display_controller_stop(void) {
    if(dashboard && remove_timer)remove_timer(dashboard,TIMER);
    int bad=display_hardware_restore()!=0;
    for(int i=4;i>=0;--i)if(memory_restore(&hooks[i]))bad=1;
    if(bad)return -1;
    display_hardware_close();dashboard=0;host=0;return 0;
}
