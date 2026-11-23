#include "window.h"
extern void *calloc(__SIZE_TYPE__,__SIZE_TYPE__);
extern void free(void *);
#define ACTIVITY "dashborneExtension_screen_mirroring"
#define BACK 299180
#define RETRY 299181
#define ASPECT 299182
#define RETRY_PANEL 119183
#define HINT 119184
#define RATIO 599185
#define VIDEO 699180
#define WAITING 119180
#define STATUS 599180
#define TIMER 929180
static const struct dashborne_host *api;
static struct mirror_session *session;
static void *table[17],*live;
static int pending,closing,failed,last_state=-1,last_aspect=-1;
static int hint_ticks;
static void (*ctor)(void *),(*dtor)(void *),(*base_created)(void *);
static void *(*find)(void *,int),*(*context)(void);
static void (*text)(void *,const char *),(*visible)(void *,int);
static void (*position)(void *,const struct mirror_rect *);
static int (*id_of)(void *);
static void (*register_timer)(void *,int,int),(*unregister_timer)(void *,int);
static void (*open_activity)(void *,const char *,void *),(*close_activity)(void *,const char *);
static const char *layout(void *s) {(void)s;return "../../../mnt/extsd/Dashborne/extensions/screen-mirroring/assets/viewer.ftu";}
static void show(void *s,int id,int value) {void *p=find(s,id);if(p)visible(p,value);}
static void render(void *s) {
    int state=failed?MIRROR_ERROR:session->state;
    int aspect=state==MIRROR_PLAYING?session->displayed_aspect:session->aspect;
    if(aspect!=last_aspect) {
        void *v=find(s,VIDEO);
        struct mirror_rect r=aspect_rect(aspect);
        if(v)position(v,&r);
        last_aspect=aspect;
    }
    if(state==last_state)return;
    last_state=state;
    if(state==MIRROR_PLAYING)api->log(api,"screen-mirroring: viewer displaying AirPlay video");
    show(s,VIDEO,state==MIRROR_PLAYING);show(s,WAITING,state!=MIRROR_PLAYING);
    show(s,RETRY_PANEL,state!=MIRROR_PLAYING);
    void *p=find(s,STATUS);
    if(p)text(p,state==MIRROR_PLAYING?"Connected":
        state==MIRROR_ERROR?"Could not switch projection. Tap the retry arrow or return to Extra.":
        state==MIRROR_CLOSED?"Mirroring stopped. Tap the retry arrow to connect again.":
        "Join the DASH Wi-Fi hotspot to connect.");
}
static int leave(void) {
    if(session_close(session)) {
        failed=1;last_state=-1;if(live)render(live);
        api->log(api,"screen-mirroring: stock-mode restore pending; Back can retry");return -1;
    }
    return 0;
}
static void back(void *s) {
    (void)s;
    if(closing || leave())return;
    closing=1;close_activity(context(),ACTIVITY);
}
static int hardware_back(void *s) {back(s);return 1;}
static void click(void *s,void *c) {
    if(closing)return;
    int id=id_of(c);
    if(id==BACK)back(s);
    else if(id==RETRY) {failed=session_open(session)!=0;last_state=-1;render(s);}
    else if(id==ASPECT) {
        session_next_aspect(session);session_tick(session);render(s);
        int aspect=session->state==MIRROR_PLAYING?session->displayed_aspect:session->aspect;
        void *p=find(s,RATIO);if(p)text(p,aspect_label(aspect));
        hint_ticks=20;show(s,HINT,1);
    }
}
static void created(void *s) {
    base_created(s);pending=0;closing=0;failed=0;last_state=-1;last_aspect=-1;
    hint_ticks=0;show(s,HINT,0);
    show(s,VIDEO,0);render(s);
    register_timer(s,TIMER,100);
}
static void resumed(void *s) {
    closing=0;failed=session_open(session)!=0;last_state=-1;render(s);
    api->log(api,failed?"screen-mirroring: viewer startup failed":"screen-mirroring: viewer opened; AirPlay requested");
}
static void paused(void *s) {
    (void)s;
    if(!leave())api->log(api,"screen-mirroring: viewer paused; stock mode restored and restart queued");
}
static int timer(void *s,int id) {
    if(id==TIMER && !closing) {
        session_tick(session);render(s);
        if(hint_ticks && !--hint_ticks)show(s,HINT,0);
    }
    return 1;
}
static int timer_secondary(void *s,int id) {return timer((char *)s-4,id);}
static void destroy_only(void *s) {
    unregister_timer(s,TIMER);leave();
    if(live==s)live=0;
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
void window_open(void) {
    if(!api || pending || live)return;
    pending=1;open_activity(context(),ACTIVITY,0);
}
int window_bind(const struct dashborne_host *host,struct mirror_session *state) {
    api=host;session=state;
#define BIND(v,s) do {*(void **)(&(v))=api->resolve(api->ui_runtime,s);if(!(v))return -1;}while(0)
    BIND(ctor,"_ZN8ActivityC1Ev");BIND(dtor,"_ZN8ActivityD1Ev");
    BIND(base_created,"_ZN7BaseApp8onCreateEv");BIND(find,"_ZN7BaseApp15findControlByIDEi");
    BIND(text,"_ZN10ZKTextView7setTextEPKc");BIND(visible,"_ZN6ZKBase10setVisibleEb");
    BIND(position,"_ZN11ZKVideoView11setPositionERK14LayoutPosition");
    BIND(id_of,"_ZNK6ZKBase5getIDEv");
    BIND(register_timer,"_ZN7BaseApp13registerTimerEii");BIND(unregister_timer,"_ZN7BaseApp15unregisterTimerEi");
    BIND(context,"_ZN13EasyUIContext11getInstanceEv");
    BIND(open_activity,"_ZN13EasyUIContext12openActivityEPKcP6Intent");
    BIND(close_activity,"_ZN13EasyUIContext13closeActivityEPKc");
#undef BIND
    void **original=api->resolve(api->ui_runtime,"_ZTV8Activity");
    if(!original || original[6]!=(void *)base_created ||
       (__PTRDIFF_TYPE__)original[12]!=-(__PTRDIFF_TYPE__)sizeof(void *))return -1;
    for(unsigned int i=0;i<17;++i)table[i]=original[i];
    table[2]=(void *)destroy_only;table[3]=(void *)destroy;table[4]=(void *)click;
    table[5]=(void *)layout;table[6]=(void *)created;table[7]=(void *)timer;
    table[9]=(void *)resumed;table[10]=(void *)paused;table[11]=(void *)hardware_back;
    table[14]=(void *)destroy_only_secondary;table[15]=(void *)destroy_secondary;table[16]=(void *)timer_secondary;
    return api->register_activity(api,ACTIVITY,create);
}
int window_cleanup(void) {
    if(session && leave())return -1;
    /* Native close is asynchronous. Never unload live callbacks or a queued open. */
    if(live || pending)return -1;
    api=0;session=0;return 0;
}
