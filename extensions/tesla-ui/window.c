#include "window.h"
#include "source.h"
#include "graph.h"
#include "road.h"
#include "road_canvas.h"
#include "sample.h"
extern void *calloc(__SIZE_TYPE__,__SIZE_TYPE__);
extern void free(void *);
extern int strcmp(const char *,const char *);
extern int snprintf(char *,__SIZE_TYPE__,const char *,...);
#define ACTIVITY "dashborneExtension_tesla_ui"
#define TIMER 97300
#define BACK 299300
#define PREVIEW 299301
#define PREVIEW_LABEL 599333
#define FRAME_MS 16
#define SAMPLE_US 100000ull
#define FRAME_BUDGET_US 16667ull
#define ASSETS "../../../mnt/extsd/Dashborne/extensions/tesla-ui/assets/"
struct rectangle {int left,top,width,height;};
static const struct dashborne_host *api;
static void *table[17],*live;
static int pending,closing,paused_state;
static int navigation_focus;
static int preview_active;
static unsigned long long preview_started;
static int rendered_speed=-2,rendered_body=-2,history_drawn,history_count;
static unsigned int history_second;
static struct cluster state;
/* Cache properties as well as pointers: even assigning the same value can
   invalidate a native widget. No allocation or control lookup in steady state. */
struct control_cache {
    void *pointer;
    struct rectangle rect;
    unsigned int rgb,known;
    int shown,image_kind;
};
static struct control_cache primary[34],fills[GRAPH_WIDTH],traces[GRAPH_WIDTH],means[35];
static struct control_cache road_controls[ROAD_CONTROLS];
static struct road_motion road_motion;
static char road_images[2][160];
static int road_drawn,road_visible,road_zones[2];
static float road_phase_drawn;
static char labels[34][100],images[34][160];
static struct graph_runs runs;
static int active_fills,active_traces;
static unsigned long long next_sample;
static struct {float from,target;unsigned long long start;int active;} power_motion;
static struct {
    unsigned long long work_us,last_tick,gap_us,max_gap_us,startup_us;
    unsigned int ticks,samples,writes,pictures,lookups,max_work_us,max_source_us,over_budget,late;
    unsigned int intervals;
} perf;
static void (*ctor)(void *),(*dtor)(void *),(*base_created)(void *);
static void *(*find)(void *,int),*(*context)(void);
static void (*text)(void *,const char *),(*picture)(void *,const char *),(*visible)(void *,int);
static void (*position)(void *,const struct rectangle *),(*color)(void *,unsigned int);
static void (*register_timer)(void *,int,int),(*unregister_timer)(void *,int);
static int (*id_of)(void *);
static void (*open_activity)(void *,const char *,void *),(*close_activity)(void *,const char *);
static const char *layout(void *s) {(void)s;return ASSETS "cluster.ftu";}
static struct control_cache *control(int id) {
    struct control_cache *c=0;
    if(id>=599300 && id<599334)c=&primary[id-599300];
    else if(id>=GRAPH_FILL_ID && id<GRAPH_FILL_ID+GRAPH_WIDTH)c=&fills[id-GRAPH_FILL_ID];
    else if(id>=GRAPH_TRACE_ID && id<GRAPH_TRACE_ID+GRAPH_WIDTH)c=&traces[id-GRAPH_TRACE_ID];
    else if(id>=602000 && id<602035)c=&means[id-602000];
    else if(id>=ROAD_ID && id<ROAD_ID+ROAD_CONTROLS)c=&road_controls[id-ROAD_ID];
    if(c && !(c->known&1)) {c->pointer=find(live,id);c->known|=1;++perf.lookups;}
    return c && c->pointer?c:0;
}
static void label(int id,const char *value) {
    struct control_cache *c=control(id);if(!c)return;
    char *old=labels[id-599300];
    if(!(c->known&16) || strcmp(old,value)) {
        text(c->pointer,value);snprintf(old,sizeof(labels[0]),"%s",value);c->known|=16;++perf.writes;
    }
}
static void show(int id,int value) {
    struct control_cache *c=control(id);if(!c)return;value=!!value;
    if(!(c->known&2) || c->shown!=value) {visible(c->pointer,value);c->shown=value;c->known|=2;++perf.writes;}
}
static void place(int id,struct rectangle r) {
    struct control_cache *c=control(id);if(!c)return;
    if(!(c->known&4) || c->rect.left!=r.left || c->rect.top!=r.top ||
        c->rect.width!=r.width || c->rect.height!=r.height) {
        position(c->pointer,&r);c->rect=r;c->known|=4;++perf.writes;
    }
}
static void tint(int id,unsigned int rgb) {
    struct control_cache *c=control(id);if(!c)return;
    if(!(c->known&8) || c->rgb!=rgb) {color(c->pointer,rgb);c->rgb=rgb;c->known|=8;++perf.writes;}
}
static void asset(int id,const char *path) {
    struct control_cache *c=control(id);if(!c)return;
    char *old=id==ROAD_ID+2 || id==ROAD_ID+3?road_images[id-ROAD_ID-2]:images[id-599300];
    if(!(c->known&32) || strcmp(old,path)) {
        picture(c->pointer,path);snprintf(old,sizeof(images[0]),"%s",path);c->known|=32;++perf.pictures;++perf.writes;
    }
}
static void bar(int id,int x,int y,int w,int h,unsigned int rgb,int on) {
    if(on) {place(id,(struct rectangle){x,y,w,h});tint(id,rgb);}
    show(id,on);
}
static float power_at(unsigned long long now) {
    if(!power_motion.active || now<power_motion.start || now-power_motion.start>=SAMPLE_US)
        return power_motion.target;
    return power_motion.from+(power_motion.target-power_motion.from)*(float)(unsigned int)(now-power_motion.start)/(float)SAMPLE_US;
}
static void paint_power(unsigned long long now) {
    float value=power_at(now);int negative=value<0;
    int width=(int)(negative?-value:value);
    bar(599323,negative?1090-width:1090,431,width,3,negative?0x66ba6b:0xd9dce0,width>0);
    if(value==power_motion.target)power_motion.active=0;
}
static void paint_road(unsigned long long now) {
    int on=state.mode==DRIVING;
    road_step(&road_motion,&state,now);
    if(!road_drawn || on!=road_visible || road_motion.phase!=road_phase_drawn) {
        show(ROAD_ID,on);show(ROAD_ID+1,on);
        if(on)road_canvas_frame(road_motion.phase,now);
        road_drawn=1;road_visible=on;road_phase_drawn=road_motion.phase;
    }
    for(int side=0;side<2;++side) {
        enum road_zone zone=road_zone(&state,side?SIDE_RIGHT:SIDE_LEFT);
        if(road_zones[side]==(int)zone)continue;
        int id=ROAD_ID+2+side;
        if(zone!=ZONE_OFF) {
            const char *names[]={"","turn","blind","alert"};char path[160];
            snprintf(path,sizeof(path),ASSETS "road-%s-%s.png",side?"right":"left",names[zone]);asset(id,path);
        }
        show(id,zone!=ZONE_OFF);
        road_zones[side]=(int)zone;
    }
}
static void graph_paint(void) {
    graph_runs(state.history,&runs);
    for(int i=0;i<runs.fills;++i) {
        struct graph_run r=runs.fill[i];int id=GRAPH_FILL_ID+i;
        place(id,(struct rectangle){GRAPH_LEFT+r.x,r.top,r.width,r.height});
        struct control_cache *c=control(id);
        if(c && (!(c->known&32) || c->image_kind!=r.negative)) {
            picture(c->pointer,r.negative?ASSETS "regen-fill.png":ASSETS "energy-fill.png");
            c->image_kind=r.negative;c->known|=32;++perf.pictures;++perf.writes;
        }
        show(id,1);
    }
    for(int i=runs.fills;i<active_fills;++i)show(GRAPH_FILL_ID+i,0);
    for(int i=0;i<runs.traces;++i) {
        struct graph_run r=runs.trace[i];
        bar(GRAPH_TRACE_ID+i,GRAPH_LEFT+r.x,r.top,r.width,r.height,r.negative?0x66ba80:0xeaa34b,1);
    }
    for(int i=runs.traces;i<active_traces;++i)show(GRAPH_TRACE_ID+i,0);
    active_fills=runs.fills;active_traces=runs.traces;
}
static void speed_digits(int speed_value) {
    if(rendered_speed==speed_value)return;
    rendered_speed=speed_value;
    int n=speed_value>=100?3:speed_value>=10?2:1;
    char digits[8];snprintf(digits,sizeof(digits),"%d",speed_value);
    for(int i=0;i<3;++i) {
        int id=599310+i;
        int on=speed_value>=0 && i<n;
        if(on) {
            char path[128];snprintf(path,sizeof(path),ASSETS "digit-%c.png",digits[i]);
            struct rectangle r={1505-n*38+i*76,140,76,148};place(id,r);asset(id,path);
        }
        show(id,on);
    }
}
static void vehicle_body(const struct readings *d,int driving,int waiting) {
    /* Only a complete verified snapshot chooses a posed model. */
    int mask=d->body_known==BODY_ALL?(int)d->body_open:-1;
    int view_key=(mask+1)*2+driving;
    if(view_key!=rendered_body) {
        int id=599306+driving;
        char path[160];const char *view=driving?"drive":"park";
        if(mask>=0)snprintf(path,sizeof(path),ASSETS "body/%s-%02x.png",view,mask);
        else snprintf(path,sizeof(path),ASSETS "%s.png",view);
        struct rectangle r=driving?(mask>=0?(struct rectangle){780,80,400,300}:
            (struct rectangle){865,125,230,260}):(struct rectangle){720,103,500,300};
        place(id,r);asset(id,path);
        rendered_body=view_key;
    }
    show(599306,!driving);show(599307,driving);
    place(599308,driving?(struct rectangle){745,390,500,30}:(struct rectangle){745,365,500,38});
    if(waiting)label(599308,d->demo?"Vehicle demo mode":"Waiting for vehicle data");
    else if(mask<0)label(599308,"Door status unavailable");
    else if(mask) {
        const char *names[]={"Front left door open","Rear left door open","Front right door open",
            "Rear right door open","Frunk open","Trunk open"};
        int count=0,last=0;
        for(int i=0;i<6;++i)if(mask&(1<<i)) {++count;last=i;}
        const char *message="Doors open";
        if(!(mask&15))message="Frunk and trunk open";
        else if((mask&48)==48)message="Doors, frunk and trunk open";
        else if(mask&FRUNK)message="Doors and frunk open";
        else if(mask&TRUNK)message="Doors and trunk open";
        label(599308,count==1?names[last]:message);
    } else label(599308,d->charging?"Charging":"");
}
static void render(unsigned long long now,int immediate) {
    struct readings raw;unsigned long long started=source_microseconds();
    if(preview_active) {
        char caption[100];
        const char *scene=sample_read(&raw,now>=preview_started?now-preview_started:0);
        snprintf(caption,sizeof(caption),"PREVIEW · %s",scene);label(PREVIEW_LABEL,caption);
    } else source_read(&raw);
    show(PREVIEW_LABEL,preview_active);
    unsigned long long ended=source_microseconds();
    unsigned int elapsed=ended>=started?(unsigned int)(ended-started):0;
    ++perf.samples;if(elapsed>perf.max_source_us)perf.max_source_us=elapsed;
    enum cluster_mode previous_mode=state.mode;
    cluster_update(&state,&raw,(unsigned int)(now/1000000));
    const struct readings *d=&state.data;char line[100];
    int driving=state.mode==DRIVING,waiting=state.mode==WAITING;
    speed_digits(driving?d->speed:-1);
    show(599309,!driving || d->speed==UNKNOWN);
    label(599309,waiting?"—":driving?"—":"P");
    label(599313,driving?(d->mph?"MPH":"KM/H"):state.mode==CHARGING?"CHARGING":waiting?"":"PARK");
    vehicle_body(d,driving,waiting);
    paint_road(now);
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
    float target=d->power_valid && driving?(d->power_kw<0?d->power_kw*355/100:d->power_kw*355/400):0;
    if(target>355)target=355;
    if(target<-355)target=-355;
    if(immediate || !d->power_valid || !driving || previous_mode!=state.mode || !now) {
        power_motion.from=power_motion.target=target;power_motion.active=0;
    } else if(target!=power_motion.target) {
        power_motion.from=power_at(now);power_motion.target=target;power_motion.start=now;power_motion.active=1;
    }
    paint_power(now);
    if(!history_drawn || history_second!=state.last_second || history_count!=state.count) {
        int average=graph_average(state.history);
        if(average!=UNKNOWN)snprintf(line,sizeof(line),"Avg. %d kW",(average+(average<0?-5:5))/10);
        else snprintf(line,sizeof(line),"— kW");
        label(599301,line);
        graph_paint();
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
/* Timing covers our callbacks and native setters, not asynchronous painting or
   scanout. Emit once when leaving the view; never write logs in the frame loop. */
static void report(void) {
    if(!api || !api->log || !perf.ticks)return;
    char message[512];
    snprintf(message,sizeof(message),
        "tesla-ui: callbacks=%u work_avg_us=%llu work_max_us=%u over_16667us=%u "
        "gap_avg_us=%llu gap_max_us=%llu gaps_over_25ms=%u samples=%u source_max_us=%u startup_us=%llu "
        "setters=%u image_changes=%u lookups=%u (callback timing, not display FPS)",
        perf.ticks,perf.work_us/perf.ticks,perf.max_work_us,perf.over_budget,
        perf.intervals?perf.gap_us/perf.intervals:0,perf.max_gap_us,perf.late,perf.samples,perf.max_source_us,perf.startup_us,
        perf.writes,perf.pictures,perf.lookups);
    api->log(api,message);perf=(__typeof__(perf)){0};
}
static void back(void *s) {
    (void)s;if(closing)return;
    preview_active=0;source_preview_awake(0);
    closing=1;unregister_timer(live,TIMER);report();close_activity(context(),ACTIVITY);
}
static void focus_back(int selected) {
    navigation_focus=selected;
    for(int i=0;i<4;++i)show(599326+i,selected);
}
int cluster_window_input(enum cluster_input action,int navigation_allowed) {
    if(!live || pending || paused_state)return 0;
    if(!navigation_allowed) {if(navigation_focus)focus_back(0);return 0;}
    /* The fixed cluster currently has one interactive control: Back. Keep
       page navigation unhandled until this view actually has multiple pages. */
    if(action<CLUSTER_PREVIOUS || action>CLUSTER_BACK)return 0;
    if(closing)return 1;
    if(action==CLUSTER_PREVIOUS || action==CLUSTER_NEXT) {focus_back(1);return 1;}
    if(action==CLUSTER_SELECT && !navigation_focus)return 0;
    back(live);return 1;
}
static int hardware_back(void *s) {(void)s;return cluster_window_input(CLUSTER_BACK,1);}
static void click(void *s,void *c) {
    if(!c || closing || paused_state)return;
    if(id_of(c)==BACK)back(s);
    else if(id_of(c)==PREVIEW) {
        preview_active=!preview_active;source_preview_awake(preview_active);
        unsigned long long now=source_microseconds();preview_started=now;
        /* Never carry sample history, poses or interpolation into live mode. */
        cluster_reset(&state);history_drawn=0;road_motion=(struct road_motion){0};
        text(c,preview_active?"Stop preview":"Preview");
        render(now,1);next_sample=now+SAMPLE_US;
    }
}
static void created(void *s) {
    base_created(s);pending=0;closing=0;paused_state=0;preview_active=0;
    for(int i=0;i<34;++i)primary[i]=(struct control_cache){0};
    for(int i=0;i<GRAPH_WIDTH;++i)fills[i]=traces[i]=(struct control_cache){0};
    for(int i=0;i<35;++i)means[i]=(struct control_cache){0};
    for(int i=0;i<ROAD_CONTROLS;++i)road_controls[i]=(struct control_cache){0};
    struct control_cache *road=control(ROAD_ID);
    if(!road || road_canvas_attach(road->pointer))api->log(api,"tesla-ui: road canvas unavailable");
    road_motion=(struct road_motion){0};
    road_drawn=0;road_zones[0]=road_zones[1]=-1;
    active_fills=active_traces=0;
    perf=(__typeof__(perf)){0};power_motion=(__typeof__(power_motion)){0};
    cluster_reset(&state);rendered_speed=-2;rendered_body=-2;history_drawn=0;
    unsigned long long now=source_microseconds();
    focus_back(0);
    /* Resolve the graph pool before starting the timer, not when the first
       steep trace suddenly needs hundreds of additional rectangles. */
    for(int i=0;i<GRAPH_WIDTH;++i) {control(GRAPH_FILL_ID+i);control(GRAPH_TRACE_ID+i);}
    render(now,1);next_sample=now+SAMPLE_US;
    unsigned long long end=source_microseconds();perf.startup_us=end>=now?end-now:0;
    register_timer(s,TIMER,FRAME_MS);
}
static void resumed(void *s) {
    if(closing)return;
    if(paused_state) {
        cluster_reset(&state);history_drawn=0;
        unsigned long long now=source_microseconds();render(now,1);next_sample=now+SAMPLE_US;
    }
    paused_state=0;perf.last_tick=0;
    unregister_timer(s,TIMER);register_timer(s,TIMER,FRAME_MS);
}
static void paused(void *s) {
    if(preview_active) {
        preview_active=0;source_preview_awake(0);show(PREVIEW_LABEL,0);
        void *button=find(s,PREVIEW);if(button)text(button,"Preview");
    }
    paused_state=1;focus_back(0);road_motion=(struct road_motion){0};unregister_timer(s,TIMER);report();
}
static int timer(void *s,int id) {
    (void)s;
    if(id!=TIMER || closing || paused_state || !live)return 1;
    unsigned long long now=source_microseconds();
    if(perf.last_tick && now>=perf.last_tick) {
        unsigned long long gap=now-perf.last_tick;
        perf.gap_us+=gap;++perf.intervals;
        if(gap>perf.max_gap_us)perf.max_gap_us=gap;
        if(gap>25000)++perf.late;
    }
    perf.last_tick=now;
    if(!now || now>=next_sample || next_sample-now>SAMPLE_US) {
        render(now,0);
        /* Keep the 100 ms sample phase, but skip missed work after a stall. */
        next_sample=now && now>=next_sample && now-next_sample<SAMPLE_US?next_sample+SAMPLE_US:now+SAMPLE_US;
    } else {
        if(power_motion.active)paint_power(now);
        if(state.mode==DRIVING)paint_road(now);
    }
    unsigned long long end=source_microseconds();
    unsigned int work=end>=now?(unsigned int)(end-now):0;
    ++perf.ticks;perf.work_us+=work;
    if(work>perf.max_work_us)perf.max_work_us=work;
    if(work>FRAME_BUDGET_US)++perf.over_budget;
    return 1;
}
static int timer_secondary(void *s,int id) {return timer((char *)s-sizeof(void *),id);}
static void destroy_only(void *s) {
    road_canvas_detach();source_preview_awake(0);
    unregister_timer(s,TIMER);report();if(live==s)live=0;pending=0;dtor(s);
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
    if(road_canvas_bind(host))return -1;
#define BIND(v,s) do {*(void **)(&(v))=api->resolve(api->ui_runtime,s);if(!(v)){api->log(api,"tesla-ui: window missing symbol " s);return -1;}}while(0)
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
       (__PTRDIFF_TYPE__)original[12]!=-(__PTRDIFF_TYPE__)sizeof(void *)){api->log(api,"tesla-ui: activity vtable mismatch");return -1;}
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
