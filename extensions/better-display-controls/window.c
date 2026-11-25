#include "controller.h"
extern void *calloc(__SIZE_TYPE__,__SIZE_TYPE__);extern void free(void *);
extern int snprintf(char *,__SIZE_TYPE__,const char *,...);
#define ACTIVITY "dashborneExtension_display_controls"
#define TIMER 97521
static const struct dashborne_host *host;
static void *table[17],*live;static int pending,closing,rendering,visible;
static void *display_controller;
static void (*reset_timeout)(void *);
static int (*manual_off)(void *);
/* Custom Activities do not pass touches through the dashboard's sleep handler. */
static void wake(void) {if(visible&&display_controller&&!manual_off(display_controller))reset_timeout(display_controller);}
static void (*ctor)(void *),(*dtor)(void *),(*base_created)(void *);
static void *(*find)(void *,int),*(*context)(void);
static void (*text)(void *,const char *),(*selected)(void *,int),(*progress)(void *,int),(*seek_listener)(void *,void *);
static int (*id_of)(void *);
static void (*add_timer)(void *,int,int),(*remove_timer)(void *,int);
static void (*open_activity)(void *,const char *,void *),(*close_activity)(void *,const char *);
static const char *layout(void *s) {(void)s;return "../../../mnt/extsd/Dashborne/extensions/better-display-controls/assets/panel.ftu";}
static void label(void *s,int id,const char *value){void *p=find(s,id);if(p)text(p,value);}
static void render(void *s) {
    rendering=1;char line[192];
    label(s,299520,display_settings.enabled?"Enabled":"Enable controls");
    void *p=find(s,299520);if(p)selected(p,display_settings.enabled);
    label(s,299521,display_settings.mode==0?"Mode: Auto":display_settings.mode==1?"Mode: Day":"Mode: Night");
    int seconds=display_preview_seconds();
    if(seconds){snprintf(line,sizeof(line),"Keep changes (%ds)",seconds);label(s,299522,line);}
    else label(s,299522,display_dirty?"Save changes":"Saved");
    label(s,599520,display_status);
    if(display_minute<0)label(s,599521,"DASH clock: waiting for sync");
    else {snprintf(line,sizeof(line),"DASH time  %02d:%02d",display_minute/60,display_minute%60);label(s,599521,line);}
    struct display_profile profiles[2]={display_settings.day,display_settings.night};
    int starts[2]={display_settings.day_start,display_settings.night_start};
    for(int i=0;i<2;i++) {
        struct display_profile v=profiles[i];
        snprintf(line,sizeof(line),"Brightness  %d.%d%%  (%d/255)",v.brightness*100/255,(v.brightness*1000/255)%10,v.brightness);
        label(s,599530+i*10,line);
        snprintf(line,sizeof(line),"Black level  %d",v.black);label(s,599531+i*10,line);
        snprintf(line,sizeof(line),"Starts at  %02d:%02d",starts[i]/60,starts[i]%60);label(s,599532+i*10,line);
        p=find(s,919530+i*10);if(p)progress(p,v.brightness-1);
        p=find(s,919531+i*10);if(p)progress(p,v.black);
    }
    rendering=0;
}
static void back(void *s) {
    if(closing)return;
    display_preview_cancel();
    if(display_dirty && !display_save()){render(s);return;}
    closing=1;close_activity(context(),ACTIVITY);
}
static int hardware_back(void *s){back(s);return 1;}
static void click(void *s,void *c) {
    if(closing)return;
    wake();
    int id=id_of(c),change=0;
    if(id==299529){back(s);return;}
    if(id==299520){
        if(display_settings.enabled){display_preview_cancel();display_settings.enabled=0;}
        else {if(!display_preview_begin())return;display_settings.enabled=1;
            if(!display_settings.mode&&display_minute<0)display_settings.mode=1;}
        change=1;
    }
    else if(id==299521){if(display_settings.enabled&&!display_preview_begin())return;display_settings.mode=(display_settings.mode+1)%3;change=1;}
    else if(id==299522){display_save();render(s);return;}
    else if(id==299523){display_preview_cancel();display_settings.enabled=0;change=1;}
    else if(id==299530||id==299531||id==299540||id==299541) {
        int night=id>=299540,delta=(id%10)?15:-15;
        int *value=night?&display_settings.night_start:&display_settings.day_start;
        int other=night?display_settings.day_start:display_settings.night_start;
        int n=display_time_step(*value,delta);if(n==other)n=display_time_step(n,delta);
        *value=n;change=1;
    }
    if(change)display_changed();render(s);
}
static void seek_changed(void *listener,void *bar,int value) {
    (void)listener;if(rendering||!live||closing)return;
    wake();
    int id=id_of(bar);struct display_profile *p=id>=919540?&display_settings.night:&display_settings.day;
    if(id==919530||id==919540){if(value<0||value>254||!display_preview_begin())return;p->brightness=value+1;}
    else if(id==919531||id==919541){if(value<0||value>100||!display_preview_begin())return;p->black=value;}
    else return;
    /* Editing a profile is an immediate, explicitly labelled manual preview. */
    display_settings.enabled=1;display_settings.mode=id>=919540?2:1;
    display_changed();render(live);
}
static void listener_destructor(void *s){(void)s;}
static void seek_tracking(void *s,void *bar){(void)s;(void)bar;}
/* Native seekbar listener: two destructors, changed, start tracking, stop tracking. */
static void *listener_table[]={(void *)listener_destructor,(void *)listener_destructor,(void *)seek_changed,(void *)seek_tracking,(void *)seek_tracking};
static void *listener=listener_table;
static void created(void *s) {
    base_created(s);pending=closing=0;visible=1;wake();
    const int ids[]={919530,919531,919540,919541};
    for(int i=0;i<4;i++){void *p=find(s,ids[i]);if(p)seek_listener(p,&listener);}
    display_tick();render(s);add_timer(s,TIMER,1000);
}
static void resumed(void *s){visible=1;wake();display_tick();render(s);}
static void paused(void *s){(void)s;visible=0;display_preview_cancel();}
static int timer(void *s,int id){if(id==TIMER&&!closing){wake();display_tick();render(s);}return 1;}
static int secondary(void *s,int id){return timer((char *)s-sizeof(void *),id);}
static void destroy_only(void *s){visible=0;display_preview_cancel();remove_timer(s,TIMER);if(live==s)live=0;pending=0;dtor(s);}
static void destroy(void *s){destroy_only(s);free(s);}
static void destroy_secondary(void *s){destroy((char *)s-sizeof(void *));}
static void destroy_only_secondary(void *s){destroy_only((char *)s-sizeof(void *));}
static void *create(void){if(live)return 0;void **p=calloc(6,sizeof(void *));if(!p){pending=0;return 0;}ctor(p);p[0]=table+2;p[1]=table+14;live=p;return p;}
void display_window_open(void){if(!host||live||pending)return;pending=1;open_activity(context(),ACTIVITY,0);}
int display_window_bind(const struct dashborne_host *h) {
    host=h;
    void *(*instance)(void)=h->resolve(h->official_library,"_ZN17DisplayController8instanceEv");
    reset_timeout=h->resolve(h->official_library,"_ZN17DisplayController19resetDisplayTimeoutEv");
    manual_off=h->resolve(h->official_library,"_ZN17DisplayController17getUserDisplayOffEv");
    if(!instance||!reset_timeout||!manual_off||!(display_controller=instance()))return -1;
#define BIND(v,s) do{*(void **)(&(v))=h->resolve(h->ui_runtime,s);if(!(v))return -1;}while(0)
    BIND(ctor,"_ZN8ActivityC1Ev");BIND(dtor,"_ZN8ActivityD1Ev");BIND(base_created,"_ZN7BaseApp8onCreateEv");
    BIND(find,"_ZN7BaseApp15findControlByIDEi");BIND(text,"_ZN10ZKTextView7setTextEPKc");
    BIND(selected,"_ZN6ZKBase11setSelectedEb");BIND(id_of,"_ZNK6ZKBase5getIDEv");
    BIND(progress,"_ZN9ZKSeekBar11setProgressEi");
    BIND(seek_listener,"_ZN9ZKSeekBar24setSeekBarChangeListenerEPNS_22ISeekBarChangeListenerE");
    BIND(add_timer,"_ZN7BaseApp13registerTimerEii");BIND(remove_timer,"_ZN7BaseApp15unregisterTimerEi");
    BIND(context,"_ZN13EasyUIContext11getInstanceEv");BIND(open_activity,"_ZN13EasyUIContext12openActivityEPKcP6Intent");
    BIND(close_activity,"_ZN13EasyUIContext13closeActivityEPKc");
#undef BIND
    void **v=h->resolve(h->ui_runtime,"_ZTV8Activity");
    if(!v||v[6]!=(void *)base_created||(__PTRDIFF_TYPE__)v[12]!=-(__PTRDIFF_TYPE__)sizeof(void *))return -1;
    for(int i=0;i<17;i++)table[i]=v[i];
    table[2]=(void *)destroy_only;table[3]=(void *)destroy;table[4]=(void *)click;table[5]=(void *)layout;
    table[6]=(void *)created;table[7]=(void *)timer;table[9]=(void *)resumed;table[10]=(void *)paused;
    table[11]=(void *)hardware_back;table[14]=(void *)destroy_only_secondary;table[15]=(void *)destroy_secondary;table[16]=(void *)secondary;
    return h->register_activity(h,ACTIVITY,create);
}
int display_window_cleanup(void){if(live||pending)return -1;host=0;return 0;}
