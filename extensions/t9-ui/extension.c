#include "dashborne_extension.h"
extern void *calloc(__SIZE_TYPE__,__SIZE_TYPE__);
extern void free(void *);
extern int open(const char *,int,...),read(int,void *,unsigned int),close(int),access(const char *,int);
extern int snprintf(char *,__SIZE_TYPE__,const char *,...);
#define ACTIVITY "dashborneExtension_t9_ui"
#define RES "/mnt/extsd/Dashborne/imports/t9-ui/viewer.ftu"
#define TIMER 929910
static const struct dashborne_host *api;
static void *table[17],*live;
static int pending,closing,paused,style;
static void (*ctor)(void *),(*dtor)(void *),(*base_created)(void *);
static void *(*find)(void *,int),*(*context)(void);
static void (*text)(void *,const char *),(*visible)(void *,int),(*progress)(void *,int);
static int (*id_of)(void *);
static void (*register_timer)(void *,int,int),(*unregister_timer)(void *,int);
static void (*open_activity)(void *,const char *,void *),(*close_activity)(void *,const char *);
static int (*speed)(void),(*battery)(void),(*range)(void),(*gear_index)(void),(*unit_index)(int);
static float (*speed_unit)(float);
static const unsigned short *flags;
static const char *gears;
static int previous[7];

static int fingerprint(const char *path,unsigned int size,unsigned int hash) {
    int fd=open(path,0);if(fd<0)return 0;
    unsigned char bytes[4096];unsigned int count=0,h=2166136261u;int n;
    while((n=read(fd,bytes,sizeof(bytes)))>0) {
        count+=(unsigned int)n;
        if(count>size)break;
        for(int i=0;i<n;++i)h=(h^bytes[i])*16777619u;
    }
    close(fd);return n==0 && count==size && h==hash;
}
static const char *layout(void *s) {(void)s;return "../../../mnt/extsd/Dashborne/imports/t9-ui/viewer.ftu";}
static void set_text(void *s,int id,const char *value) {void *p=find(s,id);if(p)text(p,value);}
static void reset_cache(void) {for(int i=0;i<7;++i)previous[i]=-99999;}
static void render(void *s) {
    int connected=(*flags&16)!=0;
    int raw_speed=connected?speed():-1,raw_range=connected?range():-1;
    int v[7]={-1,connected?battery():-1,-1,connected?gear_index():-1,
              unit_index(1),unit_index(0),connected};
    if(raw_speed>=0 && raw_speed<=400) {
        float value=speed_unit((float)raw_speed);
        if(value>=0 && value<=400)v[0]=(int)(value+0.5f);
    }
    /* SideIndicators stores range in the selected display unit already. */
    if(raw_range>=0 && raw_range<=2000)v[2]=raw_range;
    if(v[1]<0 || v[1]>100)v[1]=-1;
    int changed=0;for(int i=0;i<7;++i)if(previous[i]!=v[i])changed=1;
    if(!changed)return;
    for(int i=0;i<7;++i)previous[i]=v[i];
    int base=599910+style*20;char value[40];
    if(v[0]<0)snprintf(value,sizeof(value),"--");else snprintf(value,sizeof(value),"%d",v[0]);
    set_text(s,base,value);
    if(v[1]<0)snprintf(value,sizeof(value),"--");else snprintf(value,sizeof(value),"%d%%",v[1]);
    set_text(s,base+1,value);
    if(v[2]<0)snprintf(value,sizeof(value),"--");else snprintf(value,sizeof(value),"%d %s",v[2],v[5]?"mi":"km");
    set_text(s,base+2,value);
    char gear='-';
    if(v[3]>=0 && v[3]<4) {char c=gears[v[3]];if(c=='P'||c=='R'||c=='N'||c=='D')gear=c;}
    value[0]=gear;value[1]=0;set_text(s,base+3,value);
    set_text(s,base+4,v[4]?"MPH":"KM/H");
    void *bar=find(s,base+5);if(bar)progress(bar,v[1]<0?0:v[1]);
    set_text(s,599900,connected?"T9 prototype | Stock data":"T9 prototype | Commander disconnected");
}
static void choose(void *s) {
    for(int i=0;i<2;++i) {void *p=find(s,119910+i);if(p)visible(p,i==style);}
    reset_cache();render(s);
}
static void back(void *s) {
    if(closing)return;
    closing=1;unregister_timer(s,TIMER);close_activity(context(),ACTIVITY);
}
static int hardware_back(void *s) {back(s);return 1;}
static void click(void *s,void *c) {
    if(closing||paused)return;
    int id=id_of(c);
    if(id==299910)back(s);
    else if(id==299911) {style=1-style;choose(s);}
}
static void created(void *s) {
    base_created(s);pending=closing=paused=0;style=0;
    choose(s);register_timer(s,TIMER,200);
    api->log(api,"t9-ui: imported layouts opened with native renderer");
}
static void resumed(void *s) {
    if(closing)return;
    paused=0;reset_cache();render(s);register_timer(s,TIMER,200);
}
static void paused_view(void *s) {paused=1;unregister_timer(s,TIMER);}
static int timer(void *s,int id) {if(id==TIMER&&!closing&&!paused)render(s);return 1;}
static int timer_secondary(void *s,int id) {return timer((char *)s-4,id);}
static void destroy_only(void *s) {
    unregister_timer(s,TIMER);if(live==s)live=0;
    pending=0;dtor(s);
}
static void destroy(void *s) {destroy_only(s);free(s);}
static void destroy_only_secondary(void *s) {destroy_only((char *)s-4);}
static void destroy_secondary(void *s) {destroy((char *)s-4);}
static void *create(void) {
    if(live)return 0;
    void **p=calloc(6,sizeof(void *));if(!p) {pending=0;return 0;}
    ctor(p);p[0]=table+2;p[1]=table+14;live=p;return p;
}
static void open_view(void) {
    if(!api||pending||live)return;
    pending=1;open_activity(context(),ACTIVITY,0);
}
static int activate(const struct dashborne_host *host) {
    if(!host || host->abi_version!=DASHBORNE_EXTENSION_ABI ||
       host->struct_size<__builtin_offsetof(struct dashborne_host,register_extra)+sizeof(host->register_extra) ||
       !host->register_activity || !host->register_extra)return -1;
    api=host;
    if(access(RES,4)) {
        host->log(host,"t9-ui: import resources into Dashborne/imports/t9-ui first");return -1;
    }
    /* The connection flag and static getter ABIs are verified against this app. */
    if(!fingerprint("/res/lib/libzkgui.so",3464120u,0x095a8e8fu) ||
       !fingerprint("/lib/libeasyui.so",826052u,0x3ca7071fu)) {
        host->log(host,"t9-ui: unsupported stock app/runtime");return -1;
    }
#define BIND(v,lib,s) do {*(void **)(&(v))=host->resolve(lib,s);if(!(v))return -1;}while(0)
#define UI(v,s) BIND(v,host->ui_runtime,s)
#define APP(v,s) BIND(v,host->official_library,s)
    UI(ctor,"_ZN8ActivityC1Ev");UI(dtor,"_ZN8ActivityD1Ev");
    UI(base_created,"_ZN7BaseApp8onCreateEv");UI(find,"_ZN7BaseApp15findControlByIDEi");
    UI(text,"_ZN10ZKTextView7setTextEPKc");UI(visible,"_ZN6ZKBase10setVisibleEb");
    UI(progress,"_ZN11ZKCircleBar11setProgressEi");UI(id_of,"_ZNK6ZKBase5getIDEv");
    UI(register_timer,"_ZN7BaseApp13registerTimerEii");UI(unregister_timer,"_ZN7BaseApp15unregisterTimerEi");
    UI(context,"_ZN13EasyUIContext11getInstanceEv");
    UI(open_activity,"_ZN13EasyUIContext12openActivityEPKcP6Intent");
    UI(close_activity,"_ZN13EasyUIContext13closeActivityEPKc");
    APP(speed,"_ZN14SideIndicators8getSpeedEv");APP(battery,"_ZN14SideIndicators20getBatteryPercentageEv");
    APP(range,"_ZN14SideIndicators13getRangeValueEv");APP(gear_index,"_ZN14SideIndicators12getGearIndexEv");
    APP(flags,"_ZN10InitSystem13status_flags_E");APP(gears,"_ZN14SideIndicators5gearsE");
    APP(speed_unit,"_ZN8WdgUnits19getSpeedCurrentUnitEf");
    APP(unit_index,"_ZN8WdgUnits12getUnitIndexE10UnitType_t");
#undef APP
#undef UI
#undef BIND
    void **original=host->resolve(host->ui_runtime,"_ZTV8Activity");
    if(!original || original[6]!=(void *)base_created ||
       (__PTRDIFF_TYPE__)original[12]!=-(__PTRDIFF_TYPE__)sizeof(void *))return -1;
    for(int i=0;i<17;++i)table[i]=original[i];
    table[2]=(void *)destroy_only;table[3]=(void *)destroy;table[4]=(void *)click;
    table[5]=(void *)layout;table[6]=(void *)created;table[7]=(void *)timer;
    table[9]=(void *)resumed;table[10]=(void *)paused_view;table[11]=(void *)hardware_back;
    table[14]=(void *)destroy_only_secondary;table[15]=(void *)destroy_secondary;table[16]=(void *)timer_secondary;
    if(host->register_activity(host,ACTIVITY,create))return -1;
    return host->register_extra(host,"viewer","T9 UI","Experimental dashboard port. Tap Style to switch layouts.",open_view);
}
static int deactivate(void) {
    if(live||pending)return -1;
    api=0;return 0;
}
DASHBORNE_EXTENSION_EXPORT const struct dashborne_extension *dashborne_extension_v1(void) {
    static const struct dashborne_extension extension={DASHBORNE_EXTENSION_ABI,sizeof(extension),"t9-ui",activate,deactivate};
    return &extension;
}
