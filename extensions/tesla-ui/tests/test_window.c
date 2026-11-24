#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef snprintf
#undef snprintf
#endif
#include "../window.c"
struct control {int id,shown,has_position,has_color;struct rectangle rect;unsigned int color;char value[192],image[192];};
static struct control controls[1000];static int control_count;
static struct readings fixture;
static unsigned int seconds;
static unsigned long long subsecond;
static unsigned long long clock_extra;
static unsigned int read_cost;
static int writes,pictures,lookups;
static int opens,closes,registered,reads;
static int reports;
static char last_report[512];
void source_read(struct readings *r) {*r=fixture;++reads;clock_extra+=read_cost;}
unsigned int source_seconds(void) {return seconds;}
unsigned long long source_microseconds(void) {return seconds*1000000ull+subsecond+clock_extra;}
static void log_fake(const struct dashborne_host *h,const char *line) {
    assert(h==api);snprintf(last_report,sizeof(last_report),"%s",line);++reports;
}
static void *lookup(void *s,int id) {
    assert(s==live);++lookups;
    for(int i=0;i<control_count;++i)if(controls[i].id==id)return &controls[i];
    assert(control_count<1000);struct control *c=&controls[control_count++];c->id=id;c->shown=-1;return c;
}
static void set_text(void *p,const char *s) {++writes;snprintf(((struct control *)p)->value,192,"%s",s);}
static void set_pic(void *p,const char *s) {++writes;++pictures;snprintf(((struct control *)p)->image,192,"%s",s);}
static void set_visible(void *p,int on) {++writes;((struct control *)p)->shown=!!on;}
static void set_rect(void *p,const struct rectangle *r) {
    ++writes;assert(r->width>0 && r->height>0 && r->left>=0 && r->top>=0);
    assert(r->left+r->width<=1920 && r->top+r->height<=480);
    ((struct control *)p)->rect=*r;((struct control *)p)->has_position=1;
}
static void set_color(void *p,unsigned int c) {++writes;((struct control *)p)->color=c;((struct control *)p)->has_color=1;}
static int get_id(void *p) {return ((struct control *)p)->id;}
static void noop(void *s) {(void)s;}
static void *ctx(void) {return (void *)123;}
static void opening(void *c,const char *name,void *intent) {assert(c==ctx() && !strcmp(name,ACTIVITY) && !intent);++opens;}
static void closing_fake(void *c,const char *name) {assert(c==ctx() && !strcmp(name,ACTIVITY));++closes;}
static void reg(void *s,int id,int ms) {assert(s==live && id==TIMER && ms==16);registered=1;}
static void unreg(void *s,int id) {(void)s;assert(id==TIMER);registered=0;}
static void performance_checks(void *s) {
    fixture.connected=1;fixture.gear=4;fixture.power_valid=1;fixture.power_kw=34;fixture.speed=0;
    seconds=100;subsecond=0;
    for(int i=0;i<HISTORY;++i)state.history[i]=340;
    state.last_second=seconds;state.count=HISTORY;state.sampled=1;history_drawn=0;
    render(source_microseconds(),1);next_sample=source_microseconds()+SAMPLE_US;
    assert(active_fills==1 && active_traces==1);
    int before_writes=writes,before_pictures=pictures,before_lookups=lookups,before_reads=reads;
    for(int i=1;i<=625;++i) {subsecond=(unsigned long long)i*16000;timer(s,TIMER);}
    /* Ten seconds: 625 opportunities to animate, exactly 100 data reads,
       no native mutations or lookups for an unchanged, warmed-up view. */
    assert(reads-before_reads==100);
    assert(writes==before_writes && pictures==before_pictures && lookups==before_lookups);
    fixture.speed=72;render(source_microseconds(),1);before_writes=writes;before_pictures=pictures;
    fixture.speed=73;render(source_microseconds(),1);
    assert(writes-before_writes==1 && pictures-before_pictures==1);

    seconds=150;subsecond=0;render(source_microseconds(),1);next_sample=source_microseconds()+SAMPLE_US;
    before_writes=writes;before_pictures=pictures;before_lookups=lookups;before_reads=reads;
    float previous_phase=road_motion.phase;
    for(int i=1;i<=6;++i) {subsecond=(unsigned long long)i*16000;timer(s,TIMER);}
    assert(road_motion.phase!=previous_phase && writes>before_writes && writes-before_writes<=ROAD_MARKS*6);
    assert(pictures==before_pictures && lookups==before_lookups && reads==before_reads);

    seconds=200;subsecond=0;fixture.power_kw=0;render(source_microseconds(),1);
    next_sample=source_microseconds()+SAMPLE_US;
    fixture.power_kw=400;subsecond=100000;timer(s,TIMER);assert(power_motion.active);
    int previous=0;before_reads=reads;
    for(int i=1;i<=6;++i) {
        subsecond=100000+(unsigned long long)i*16000;timer(s,TIMER);
        struct control *meter=lookup(s,599323);
        assert(meter->shown && meter->rect.width>previous && meter->rect.width<355);
        previous=meter->rect.width;
    }
    assert(reads==before_reads); /* Animation does not read telemetry. */
    subsecond=212000;timer(s,TIMER);
    assert(!power_motion.active && ((struct control *)lookup(s,599323))->rect.width==355);
    fixture.connected=0;subsecond=312000;timer(s,TIMER);
    assert(!power_motion.active && !((struct control *)lookup(s,599323))->shown);
    assert(state.mode==WAITING && !active_fills && !active_traces);
    assert(!((struct control *)lookup(s,ROAD_ID))->shown);
    for(int i=0;i<ROAD_MARKS;++i)assert(!((struct control *)lookup(s,ROAD_ID+4+i))->shown);

    /* A delayed event samples once and skips missed callbacks; no catch-up loop. */
    before_reads=reads;seconds+=120;timer(s,TIMER);assert(reads==before_reads+1);
    assert(next_sample>source_microseconds() && next_sample-source_microseconds()<=SAMPLE_US);
    /* Defend against a clock reset/failure without leaving stale data forever. */
    seconds=0;subsecond=0;timer(s,TIMER);assert(reads==before_reads+2);
    /* Instrumentation must flag slow callbacks and scheduling stalls; it must
       not mistake a timer invocation for a presented display frame. */
    perf=(__typeof__(perf)){0};seconds=400;next_sample=source_microseconds();read_cost=20000;
    timer(s,TIMER);assert(perf.over_budget==1 && perf.max_work_us==20000 && perf.max_source_us==20000);
    subsecond=100000;timer(s,TIMER);assert(perf.late==1 && perf.max_gap_us==120000);
    assert(!reports);paused(s);assert(reports==1);
    assert(strstr(last_report,"over_16667us=2") && strstr(last_report,"not display FPS"));
    read_cost=0;clock_extra=0;resumed(s);
}
static void dump(const char *name) {
    printf("{\"name\":\"%s\",\"controls\":[",name);
    for(int i=0;i<control_count;++i) {
        struct control *c=&controls[i];
        printf("%s{\"id\":%d,\"visible\":%d,\"text\":\"%s\",\"image\":\"%s\"",i?",":"",c->id,c->shown,c->value,c->image);
        if(c->has_position)printf(",\"position\":{\"left\":%d,\"top\":%d,\"width\":%d,\"height\":%d}",c->rect.left,c->rect.top,c->rect.width,c->rect.height);
        if(c->has_color)printf(",\"color\":%u",c->color);
        printf("}");
    }
    puts("]}");
}
int main(int argc,char **argv) {
    int previews=argc>1 && !strcmp(argv[1],"--preview");struct dashborne_host h={.log=log_fake};api=&h;find=lookup;text=set_text;picture=set_pic;
    visible=set_visible;position=set_rect;color=set_color;id_of=get_id;context=ctx;
    open_activity=opening;close_activity=closing_fake;ctor=noop;dtor=noop;base_created=noop;
    register_timer=reg;unregister_timer=unreg;
    fixture=(struct readings){.connected=1,.gear=4,.speed=72,.limit=80,.battery=68,.range=312,
        .temperature=21,.body_known=BODY_ALL,.power_valid=1,.power_kw=34,.nav_valid=1,.nav_distance=42,.nav_minutes=36,.nav_battery=57};
    snprintf(fixture.clock,sizeof(fixture.clock),"11:42");
    cluster_window_open();cluster_window_open();assert(opens==1 && pending && cluster_window_cleanup());
    void *s=create();assert(s && !create());created(s);resumed(s);
    assert(!pending && registered && state.mode==DRIVING);
    /* A deterministic acceleration/coasting/regen drive, only in test fixtures. */
    const int power[]={12,16,14,20,38,65,57,29,18,21,16,8,-12,-27,-18,3,14,25,21,18,34};
    for(seconds=1;seconds<=60;++seconds) {
        int i=(seconds-1)/3,t=(seconds-1)%3;
        fixture.power_kw=(float)(power[i]+(power[i+1]-power[i])*t/3);timer(s,TIMER);
    }
    fixture.power_kw=34;render(source_microseconds(),1);
    assert(!cluster_window_input(CLUSTER_SELECT,1));
    assert(!cluster_window_input(CLUSTER_NEXT,0) && !navigation_focus);
    assert(!cluster_window_input(CLUSTER_PREVIOUS_PAGE,1));
    assert(!cluster_window_input(CLUSTER_NEXT_PAGE,1));
    assert(!cluster_window_input((enum cluster_input)1000,1));
    assert(((struct control *)lookup(s,599309))->shown==0);
    assert(((struct control *)lookup(s,599310))->shown==1);
    assert(((struct control *)lookup(s,599312))->shown==0);
    if(previews)dump("drive");
    assert(((struct control *)lookup(s,ROAD_ID))->shown);
    assert(!((struct control *)lookup(s,ROAD_ID+2))->shown);
    fixture.left=1;render(source_microseconds(),1);
    assert(strstr(((struct control *)lookup(s,ROAD_ID+2))->image,"road-left-turn.png"));
    if(previews)dump("turn-left");
    fixture.left=0;fixture.blind_known=SIDES_ALL;fixture.blind_active=SIDE_LEFT;
    render(source_microseconds(),1);
    assert(strstr(((struct control *)lookup(s,ROAD_ID+2))->image,"road-left-blind.png"));
    if(previews)dump("blind-left");
    fixture.left=1;render(source_microseconds(),1);
    assert(strstr(((struct control *)lookup(s,ROAD_ID+2))->image,"road-left-alert.png"));
    if(previews)dump("blind-left-turn");
    fixture.left=0;fixture.blind_active=fixture.blind_urgent=SIDE_RIGHT;
    render(source_microseconds(),1);
    assert(!((struct control *)lookup(s,ROAD_ID+2))->shown);
    assert(strstr(((struct control *)lookup(s,ROAD_ID+3))->image,"road-right-alert.png"));
    if(previews)dump("blind-right-alert");
    fixture.blind_known=fixture.blind_active=fixture.blind_urgent=0;
    fixture.left=fixture.right=1;render(source_microseconds(),1);
    if(previews)dump("hazards");
    fixture.left=fixture.right=0;render(source_microseconds(),1);
    assert(!((struct control *)lookup(s,ROAD_ID+2))->shown && !((struct control *)lookup(s,ROAD_ID+3))->shown);
    if(argc>1 && !strcmp(argv[1],"--motion")) {
        /* Preview-only sequence: none of these blind-spot values feed the adapter. */
        for(int frame=0;frame<90;++frame) {
            subsecond=(unsigned long long)(frame+1)*32000;
            fixture.blind_known=SIDES_ALL;
            fixture.blind_active=frame>=30 && frame<75?SIDE_LEFT:0;
            fixture.left=frame>=50 && (frame/15)%2;
            timer(s,TIMER);dump("motion");
        }
        fixture.blind_known=fixture.blind_active=0;fixture.left=0;subsecond=0;
        render(source_microseconds(),1);
    }
    assert(cluster_window_input(CLUSTER_NEXT,1) && navigation_focus);
    assert(((struct control *)lookup(s,599326))->shown);
    if(previews)dump("navigation-focus");
    int prior_writes=writes;
    assert(cluster_window_input(CLUSTER_PREVIOUS,1) && writes==prior_writes);
    assert(!cluster_window_input(CLUSTER_SELECT,0) && !navigation_focus);
    fixture.gear=1;fixture.nav_valid=0;render(source_microseconds(),1);assert(state.mode==PARKED);
    assert(!((struct control *)lookup(s,ROAD_ID))->shown);
    if(previews)dump("park");
    fixture.body_open=FRONT_RIGHT;render(source_microseconds(),1);
    assert(strstr(((struct control *)lookup(s,599306))->image,"park-04.png"));
    assert(!strcmp(((struct control *)lookup(s,599308))->value,"Front right door open"));
    if(previews)dump("door-open");
    fixture.body_open=BODY_ALL;render(source_microseconds(),1);
    assert(strstr(((struct control *)lookup(s,599306))->image,"park-3f.png"));
    if(previews)dump("all-open");
    fixture.gear=4;render(source_microseconds(),1);
    assert(((struct control *)lookup(s,599307))->shown);
    assert(strstr(((struct control *)lookup(s,599307))->image,"drive-3f.png"));
    if(previews)dump("drive-open");
    fixture.gear=1;fixture.body_known=0;render(source_microseconds(),1);
    assert(strstr(((struct control *)lookup(s,599306))->image,"/park.png"));
    assert(!strcmp(((struct control *)lookup(s,599308))->value,"Door status unavailable"));
    if(previews)dump("body-unknown");
    fixture.body_known=BODY_ALL;fixture.body_open=0;
    fixture.charging=1;fixture.power_kw=11;render(source_microseconds(),1);assert(state.mode==CHARGING);
    if(previews)dump("charge");
    fixture.gear=4;fixture.charging=0;fixture.power_kw=-28;render(source_microseconds(),1);
    assert(((struct control *)lookup(s,599323))->color==0x66ba6b);
    if(previews)dump("regen");
    fixture.connected=0;render(source_microseconds(),1);assert(state.mode==WAITING && state.count==0);
    if(previews)dump("disconnected");
    performance_checks(s);
    assert(cluster_window_input(CLUSTER_NEXT,1));
    paused(s);int before=reads;timer(s,TIMER);assert(reads==before && !registered && !navigation_focus);
    assert(!cluster_window_input(CLUSTER_BACK,1) && !hardware_back(s) && !closes);
    resumed(s);assert(registered && reads>before);
    assert(cluster_window_cleanup());
    assert(!navigation_focus && !cluster_window_input(CLUSTER_SELECT,1));
    assert(cluster_window_input(CLUSTER_NEXT,1));
    assert(cluster_window_input(CLUSTER_SELECT,1));
    assert(cluster_window_input(CLUSTER_SELECT,1));
    struct control b={.id=BACK};click(s,&b);click(s,&b);assert(closes==1 && !registered);
    before=reads;timer(s,TIMER);assert(reads==before);
    assert(cluster_window_cleanup());destroy(s);assert(!cluster_window_cleanup());
    assert(!live && !pending && !registered);
    assert(!cluster_window_input(CLUSTER_BACK,1));
    /* A new native Activity needs fresh pointers and cached properties. */
    api=&h;control_count=0;memset(controls,0,sizeof(controls));
    s=create();int previous_lookups=lookups;created(s);assert(lookups>previous_lookups);
    assert(!navigation_focus && hardware_back(s) && closes==2);
    destroy(s);assert(!cluster_window_cleanup());
    return 0;
}
