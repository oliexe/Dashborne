#include "window.h"
#include "source.h"
#include "graph.h"
extern void *calloc(__SIZE_TYPE__,__SIZE_TYPE__);
extern void free(void *);
extern int snprintf(char *,__SIZE_TYPE__,const char *,...);
#define ACTIVITY "dashborneExtension_tesla_ui"
#define TIMER 97300
#define BACK 299300
#define ASSETS "../../../mnt/extsd/Dashborne/extensions/tesla-ui/assets/"
struct rectangle {int left,top,width,height;};
static const struct dashborne_host *api;
static void *table[17],*live;
static int pending,closing,paused_state;
static int rendered_speed=-2,history_drawn,history_count;
static unsigned int history_second;
static struct cluster state;
static struct {void *fill,*trace;int negative;} graph_controls[GRAPH_WIDTH];
static void (*ctor)(void *),(*dtor)(void *),(*base_created)(void *);
static void *(*find)(void *,int),*(*context)(void);
static void (*text)(void *,const char *),(*picture)(void *,const char *),(*visible)(void *,int);
static void (*position)(void *,const struct rectangle *),(*color)(void *,unsigned int);
static void (*register_timer)(void *,int,int),(*unregister_timer)(void *,int);
static int (*id_of)(void *);
static void (*open_activity)(void *,const char *,void *),(*close_activity)(void *,const char *);
static const char *layout(void *s) {(void)s;return ASSETS "cluster.ftu";}
static void label(int id,const char *value) {void *p=find(live,id);if(p)text(p,value);}
static void show(int id,int value) {void *p=find(live,id);if(p)visible(p,value);}
static void bar(int id,int x,int y,int w,int h,unsigned int rgb,int on) {
    void *p=find(live,id);if(!p)return;
    if(on) {struct rectangle r={x,y,w,h};position(p,&r);color(p,rgb);}
    visible(p,on);
}
static void speed_digits(int speed_value) {
    if(rendered_speed==speed_value)return;
    rendered_speed=speed_value;
    int n=speed_value>=100?3:speed_value>=10?2:1;
    char digits[8];snprintf(digits,sizeof(digits),"%d",speed_value);
    for(int i=0;i<3;++i) {
        void *p=find(live,599310+i);if(!p)continue;
        int on=speed_value>=0 && i<n;
        if(on) {
            char path[128];snprintf(path,sizeof(path),ASSETS "digit-%c.png",digits[i]);
            struct rectangle r={1505-n*38+i*76,140,76,148};position(p,&r);picture(p,path);
        }
        visible(p,on);
    }
}
static void render(void) {
    struct readings raw;source_read(&raw);cluster_update(&state,&raw,source_seconds());
    const struct readings *d=&state.data;char line[100];
    int driving=state.mode==DRIVING,waiting=state.mode==WAITING;
    speed_digits(driving?d->speed:-1);
    show(599309,!driving || d->speed==UNKNOWN);
    label(599309,waiting?"—":driving?"—":"P");
    label(599313,driving?(d->mph?"MPH":"KM/H"):state.mode==CHARGING?"CHARGING":waiting?"":"PARK");
    show(599306,!driving);show(599307,driving);
    label(599308,waiting?(d->demo?"Vehicle demo mode":"Waiting for vehicle data"):state.mode==CHARGING?"Charging":"");
    show(599314,driving && d->limit!=UNKNOWN);
    if(driving && d->limit!=UNKNOWN)snprintf(line,sizeof(line),"%d",d->limit);else line[0]=0;
    label(599315,line);
    if(d->range!=UNKNOWN)snprintf(line,sizeof(line),"%d %s",d->range,d->miles?"mi":"km");
    else if(d->battery!=UNKNOWN)snprintf(line,sizeof(line),"%d%%",d->battery);
    else snprintf(line,sizeof(line),"—");
    label(599318,line);
    if(d->temperature!=UNKNOWN)snprintf(line,sizeof(line),"%d°%s",d->temperature,d->fahrenheit?"F":"C");
    else snprintf(line,sizeof(line),"—");
    label(599319,line);
    line[0]=d->gear>=1 && d->gear<=4?"PRND"[d->gear-1]:0;line[1]=0;label(599320,line);
    label(599321,d->clock);
    bar(599317,188,446,d->battery>0?(d->battery*24+99)/100:1,11,
        d->battery<=20?0xd5a242:0x66ba6b,d->battery>0);
    show(599324,d->left);show(599325,d->right);
    label(599300,state.mode==CHARGING?"Charging power":"Power");
    int regen=d->power_kw<0;
    int width=d->power_valid?(int)(regen?-d->power_kw*355/100:d->power_kw*355/400):0;
    if(width>355)width=355;
    bar(599323,regen?1090-width:1090,431,width,3,regen?0x66ba6b:0xd9dce0,width>0 && driving);
    if(!history_drawn || history_second!=state.last_second || history_count!=state.count) {
        int average=graph_average(state.history);
        if(average!=UNKNOWN)snprintf(line,sizeof(line),"Avg. %d kW",(average+(average<0?-5:5))/10);
        else snprintf(line,sizeof(line),"— kW");
        label(599301,line);
        for(int x=0;x<GRAPH_WIDTH;++x) {
            struct graph_column c=graph_column(state.history,x);
            if(!graph_controls[x].fill) {
                graph_controls[x].fill=find(live,GRAPH_FILL_ID+x);
                graph_controls[x].trace=find(live,GRAPH_TRACE_ID+x);
                graph_controls[x].negative=-1;
            }
            void *fill=graph_controls[x].fill,*trace=graph_controls[x].trace;
            if(fill) {
                if(c.valid && c.fill_height) {
                    struct rectangle r={GRAPH_LEFT+x,c.fill_top,1,c.fill_height};position(fill,&r);
                    if(graph_controls[x].negative!=c.negative) {
                        picture(fill,c.negative?ASSETS "regen-fill.png":ASSETS "energy-fill.png");
                        graph_controls[x].negative=c.negative;
                    }
                }
                visible(fill,c.valid && c.fill_height);
            }
            if(trace) {
                if(c.valid) {
                    struct rectangle r={GRAPH_LEFT+x,c.top,1,c.bottom-c.top+1};position(trace,&r);
                    color(trace,c.negative?0x66ba80:0xeaa34b);
                }
                visible(trace,c.valid);
            }
        }
        for(int i=0;i<35;++i)
            bar(602000+i,GRAPH_LEFT+i*12,average!=UNKNOWN?graph_y(average):GRAPH_ZERO,6,1,
                0x85817a,average!=UNKNOWN);
        history_drawn=1;history_second=state.last_second;history_count=state.count;
    }
    if(d->nav_valid) {
        int minutes=(int)(d->nav_minutes+0.5f);
        if(minutes>=60)snprintf(line,sizeof(line),"%d hr %d min   ·   %d %s",minutes/60,minutes%60,(int)(d->nav_distance+0.5f),d->miles?"mi":"km");
        else snprintf(line,sizeof(line),"%d min   ·   %d %s",minutes,(int)(d->nav_distance+0.5f),d->miles?"mi":"km");
        char navigation[100];snprintf(navigation,sizeof(navigation),"%s   ·   %d%%",line,(int)(d->nav_battery+0.5f));
        label(599304,navigation);
    } else label(599304,"");
}
static void back(void *s) {
    (void)s;if(closing)return;
    closing=1;unregister_timer(live,TIMER);close_activity(context(),ACTIVITY);
}
static int hardware_back(void *s) {back(s);return 1;}
static void click(void *s,void *c) {if(c && id_of(c)==BACK)back(s);}
static void created(void *s) {
    base_created(s);pending=0;closing=0;paused_state=0;
    for(int x=0;x<GRAPH_WIDTH;++x) {graph_controls[x].fill=0;graph_controls[x].trace=0;}
    cluster_reset(&state);rendered_speed=-2;history_drawn=0;render();register_timer(s,TIMER,100);
}
static void resumed(void *s) {
    if(closing)return;
    if(paused_state) {cluster_reset(&state);history_drawn=0;}
    paused_state=0;render();unregister_timer(s,TIMER);register_timer(s,TIMER,100);
}
static void paused(void *s) {paused_state=1;unregister_timer(s,TIMER);}
static int timer(void *s,int id) {(void)s;if(id==TIMER && !closing && !paused_state && live)render();return 1;}
static int timer_secondary(void *s,int id) {return timer((char *)s-sizeof(void *),id);}
static void destroy_only(void *s) {
    unregister_timer(s,TIMER);if(live==s)live=0;pending=0;dtor(s);
}
static void destroy(void *s) {destroy_only(s);free(s);}
static void destroy_only_secondary(void *s) {destroy_only((char *)s-sizeof(void *));}
static void destroy_secondary(void *s) {destroy((char *)s-sizeof(void *));}
static void *create(void) {
    if(live)return 0;
    void **p=calloc(6,sizeof(void *));if(!p) {pending=0;return 0;}
    ctor(p);p[0]=table+2;p[1]=table+14;live=p;return p;
}
void cluster_window_open(void) {
    if(!api || pending || live)return;
    pending=1;open_activity(context(),ACTIVITY,0);
}
int cluster_window_bind(const struct dashborne_host *host) {
    api=host;
#define BIND(v,s) do {*(void **)(&(v))=api->resolve(api->ui_runtime,s);if(!(v))return -1;}while(0)
    BIND(ctor,"_ZN8ActivityC1Ev");BIND(dtor,"_ZN8ActivityD1Ev");
    BIND(base_created,"_ZN7BaseApp8onCreateEv");BIND(find,"_ZN7BaseApp15findControlByIDEi");
    BIND(text,"_ZN10ZKTextView7setTextEPKc");BIND(visible,"_ZN6ZKBase10setVisibleEb");
    BIND(picture,"_ZN6ZKBase16setBackgroundPicEPKc");
    BIND(position,"_ZN6ZKBase11setPositionERK14LayoutPosition");BIND(color,"_ZN6ZKBase18setBackgroundColorEj");
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
int cluster_window_cleanup(void) {
    if(live || pending)return -1;
    api=0;return 0;
}
