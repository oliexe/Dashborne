#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#undef snprintf
#include "../extension.c"
static int calls,starts,stops,closed,opened,log_count,freed;
static unsigned short status=16;
static char texts[6][40];
static int value_speed=51,value_battery=78,value_range=240,value_gear=3;
static int imperial;
static int get_speed(void){return value_speed;}
static int get_battery(void){return value_battery;}
static int get_range(void){return value_range;}
static int get_gear(void){return value_gear;}
static int units(int i){(void)i;return imperial;}
static float convert(float v){return imperial?v*0.621371f:v;}
static void *lookup(void *p,int id){(void)p;return (void *)(long)id;}
static void txt(void *p,const char *s){int i=(int)(long)p-599910-style*20;if(i>=0&&i<6)snprintf(texts[i],40,"%s",s);++calls;}
static void vis(void *p,int v){(void)p;(void)v;}
static void prog(void *p,int v){(void)p;assert(v>=0&&v<=100);}
static void reg(void *p,int i,int ms){(void)p;assert(i==TIMER&&ms==200);++starts;}
static void unreg(void *p,int i){(void)p;assert(i==TIMER);++stops;}
static void *ctx(void){return (void *)1;}
static void close_view(void *p,const char *name){(void)p;assert(!strcmp(name,ACTIVITY));++closed;}
static void open_test(void *p,const char *name,void *intent){(void)p;(void)name;(void)intent;++opened;}
static void noop(void *p){(void)p;}
static void dead(void *p){(void)p;++freed;}
static void log_test(const struct dashborne_host *h,const char *s){(void)h;(void)s;++log_count;}
int main(void){
    struct dashborne_host host={.log=log_test};api=&host;
    flags=&status;gears="PRND";speed=get_speed;battery=get_battery;range=get_range;gear_index=get_gear;
    unit_index=units;speed_unit=convert;find=lookup;text=txt;visible=vis;progress=prog;
    register_timer=reg;unregister_timer=unreg;context=ctx;close_activity=close_view;open_activity=open_test;
    base_created=noop;dtor=dead;live=(void *)1;
    created(live);assert(log_count==1&&starts==1);
    assert(!strcmp(texts[0],"51")&&!strcmp(texts[1],"78%")&&!strcmp(texts[3],"D"));
    int n=calls;timer(live,TIMER);assert(calls==n);
    imperial=1;timer(live,TIMER);
    assert(!strcmp(texts[0],"32")&&!strcmp(texts[2],"240 mi")&&!strcmp(texts[4],"MPH"));
    style=1;choose(live);assert(!strcmp(texts[0],"32"));
    style=0;imperial=0;choose(live);
    status=0;timer(live,TIMER);assert(!strcmp(texts[0],"--")&&!strcmp(texts[3],"-"));
    status=16;value_speed=900;value_battery=999;value_range=-7;value_gear=99;
    timer(live,TIMER);assert(!strcmp(texts[0],"--")&&!strcmp(texts[1],"--")&&!strcmp(texts[3],"-"));
    paused_view(live);n=calls;value_speed=20;timer(live,TIMER);assert(calls==n);
    resumed(live);assert(!strcmp(texts[0],"20"));
    assert(deactivate()==-1);back(live);back(live);assert(closed==1);
    n=calls;timer(live,TIMER);assert(calls==n);
    destroy_only(live);assert(freed==1&&live==0&&deactivate()==0);
    api=&host;open_view();open_view();assert(opened==1&&deactivate()==-1);
    puts("PASS: cached updates, missing data, invalid values, pause/resume, Back and unload lifetime");
}
