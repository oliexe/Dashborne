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
static int opens,closes,registered,reads;
void source_read(struct readings *r) {*r=fixture;++reads;}
unsigned int source_seconds(void) {return seconds;}
static void *lookup(void *s,int id) {
    assert(s==live);
    for(int i=0;i<control_count;++i)if(controls[i].id==id)return &controls[i];
    assert(control_count<1000);struct control *c=&controls[control_count++];c->id=id;c->shown=-1;return c;
}
static void set_text(void *p,const char *s) {snprintf(((struct control *)p)->value,192,"%s",s);}
static void set_pic(void *p,const char *s) {snprintf(((struct control *)p)->image,192,"%s",s);}
static void set_visible(void *p,int on) {((struct control *)p)->shown=!!on;}
static void set_rect(void *p,const struct rectangle *r) {
    assert(r->width>0 && r->height>0 && r->left>=0 && r->top>=0);
    assert(r->left+r->width<=1920 && r->top+r->height<=480);
    ((struct control *)p)->rect=*r;((struct control *)p)->has_position=1;
}
static void set_color(void *p,unsigned int c) {((struct control *)p)->color=c;((struct control *)p)->has_color=1;}
static int get_id(void *p) {return ((struct control *)p)->id;}
static void noop(void *s) {(void)s;}
static void *ctx(void) {return (void *)123;}
static void opening(void *c,const char *name,void *intent) {assert(c==ctx() && !strcmp(name,ACTIVITY) && !intent);++opens;}
static void closing_fake(void *c,const char *name) {assert(c==ctx() && !strcmp(name,ACTIVITY));++closes;}
static void reg(void *s,int id,int ms) {assert(s==live && id==TIMER && ms==100);registered=1;}
static void unreg(void *s,int id) {(void)s;assert(id==TIMER);registered=0;}
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
    (void)argv;struct dashborne_host h={0};api=&h;find=lookup;text=set_text;picture=set_pic;
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
    fixture.power_kw=34;render();
    assert(((struct control *)lookup(s,599309))->shown==0);
    assert(((struct control *)lookup(s,599310))->shown==1);
    assert(((struct control *)lookup(s,599312))->shown==0);
    if(argc>1)dump("drive");
    fixture.gear=1;fixture.nav_valid=0;render();assert(state.mode==PARKED);
    if(argc>1)dump("park");
    fixture.body_open=FRONT_RIGHT;render();
    assert(strstr(((struct control *)lookup(s,599306))->image,"park-04.png"));
    assert(!strcmp(((struct control *)lookup(s,599308))->value,"Front right door open"));
    if(argc>1)dump("door-open");
    fixture.body_open=BODY_ALL;render();
    assert(strstr(((struct control *)lookup(s,599306))->image,"park-3f.png"));
    if(argc>1)dump("all-open");
    fixture.gear=4;render();
    assert(((struct control *)lookup(s,599307))->shown);
    assert(strstr(((struct control *)lookup(s,599307))->image,"drive-3f.png"));
    if(argc>1)dump("drive-open");
    fixture.gear=1;fixture.body_known=0;render();
    assert(strstr(((struct control *)lookup(s,599306))->image,"/park.png"));
    assert(!strcmp(((struct control *)lookup(s,599308))->value,"Door status unavailable"));
    if(argc>1)dump("body-unknown");
    fixture.body_known=BODY_ALL;fixture.body_open=0;
    fixture.charging=1;fixture.power_kw=11;render();assert(state.mode==CHARGING);
    if(argc>1)dump("charge");
    fixture.gear=4;fixture.charging=0;fixture.power_kw=-28;render();
    assert(((struct control *)lookup(s,599323))->color==0x66ba6b);
    if(argc>1)dump("regen");
    fixture.connected=0;render();assert(state.mode==WAITING && state.count==0);
    if(argc>1)dump("disconnected");
    paused(s);int before=reads;timer(s,TIMER);assert(reads==before && !registered);
    resumed(s);assert(registered && reads>before);
    assert(cluster_window_cleanup());
    struct control b={.id=BACK};click(s,&b);click(s,&b);assert(closes==1 && !registered);
    before=reads;timer(s,TIMER);assert(reads==before);
    assert(cluster_window_cleanup());destroy(s);assert(!cluster_window_cleanup());
    assert(!live && !pending && !registered);
}
