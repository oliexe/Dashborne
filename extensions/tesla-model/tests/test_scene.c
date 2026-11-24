#include "../scene.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned long long now=1000000;
static void advance(struct tm_scene *s,struct tm_reading *r,int ticks) {
    for(int i=0;i<ticks;++i){now+=25000;tm_step(s,r,now);assert(s->angle>=0&&s->angle<=48);}
}
int main(void) {
    struct tm_scene s={0};struct tm_reading r={.connected=1,.gear=TM_PARK,.known=63};
    assert(tm_step(&s,&r,now));assert(s.angle==0);
    assert(!tm_step(&s,&r,now+1));
    r.gear=TM_DRIVE;r.speed=30;advance(&s,&r,24);assert(s.angle==24);
    advance(&s,&r,24);assert(s.angle==48);
    int old=s.road;advance(&s,&r,1);advance(&s,&r,1);assert(s.road!=old);
    r.gear=TM_REVERSE;advance(&s,&r,24);assert(s.angle==24);
    r.speed=0;advance(&s,&r,1);old=s.road;advance(&s,&r,100);assert(s.road==old);
    /* Interrupt a transition with an open door. Camera returns before the
       panel moves, and the driving camera cannot hide an unclosed door. */
    r.gear=TM_DRIVE;advance(&s,&r,10);assert(s.angle>24&&s.angle<48);
    old=s.angle;r.open=1;advance(&s,&r,10);assert(s.angle<old&&s.panels[0]==0);
    advance(&s,&r,60);assert(s.angle==0&&s.panels[0]==15);
    r.known=62;r.open=0;advance(&s,&r,20);assert(s.panels[0]==15&&s.data.open==1);
    r.known=63;
    r.open=0;advance(&s,&r,17);assert(s.angle==0&&s.panels[0]>0);
    advance(&s,&r,1);assert(s.angle==0&&s.panels[0]==0);
    advance(&s,&r,48);assert(s.angle==48);
    /* Every combination of six panels must compose without overflow. */
    struct tm_layer layers[TM_LAYERS];
    for(unsigned int mask=0;mask<64;++mask) {
        s=(struct tm_scene){0};r.gear=TM_PARK;r.open=mask;
        tm_step(&s,&r,now);int count=tm_layers(&s,layers);assert(count>0&&count<=TM_LAYERS);
        for(int p=0;p<6;++p)assert(s.panels[p]==((mask>>p&1)?15:0));
        for(int i=0;i<count;++i)assert(layers[i].w>0&&layers[i].h>0&&layers[i].frame>=-1&&layers[i].frame<25);
    }
    r.gear=TM_DRIVE;r.open=0;r.left=r.right=1;r.lights_known=r.high_beam=1;
    s=(struct tm_scene){0};tm_step(&s,&r,now);int count=tm_layers(&s,layers);assert(count==3);
    struct tm_lamp lamps[4];assert(tm_lamps(&s,lamps)==2&&lamps[0].y0==272);
    s.angle=0;assert(tm_lamps(&s,lamps)==2&&lamps[0].color==0xffa52d);
    s.data.left=s.data.right=0;assert(tm_lamps(&s,lamps)==2&&lamps[0].color==0xf4f8ff);
    s.data.lights_known=0;assert(!tm_lamps(&s,lamps));
    r.connected=0;advance(&s,&r,100);assert(s.angle==0&&s.data.left==0&&!s.data.lights_known&&!tm_lamps(&s,lamps));
    r=(struct tm_reading){.connected=1,.gear=999,.speed=999,.known=0,.open=63};
    advance(&s,&r,10);assert(!s.data.open&&!s.data.speed&&s.angle==0);
    /* A long pause or backward clock must not create a burst of animation. */
    r.gear=TM_DRIVE;advance(&s,&r,4);old=s.angle;
    now+=90000000;tm_step(&s,&r,now);assert(s.angle==old);
    tm_step(&s,&r,1);assert(s.angle==old);
    /* Camera motion eases at both ends, with no road decode churn during the
       rotation. Retargeting mid-turn starts at the current sub-frame position. */
    s=(struct tm_scene){0};r=(struct tm_reading){.connected=1,.gear=TM_PARK,.known=63};
    tm_step(&s,&r,now);r.gear=TM_DRIVE;r.speed=50;
    advance(&s,&r,4);assert(s.angle<4&&s.road==0);
    advance(&s,&r,20);assert(s.angle==24&&s.road==0);
    old=s.angle;int value=s.camera.value;r.gear=TM_PARK;
    tm_step(&s,&r,now);assert(s.angle==old&&s.camera.value==value);
    for(int i=0;i<24;++i){advance(&s,&r,1);assert(s.angle<=old);old=s.angle;}
    assert(s.angle==0&&s.camera.value==0);
    /* Irregular update intervals yield the same pose, without accumulating
       timing drift. Exercise repeated reversals, panels and clock resets. */
    struct tm_scene regular={0},jitter={0};r.gear=TM_PARK;
    tm_step(&regular,&r,1000000);tm_step(&jitter,&r,1000000);r.gear=TM_DRIVE;
    for(unsigned long long t=1025000;t<=2200000;t+=25000)tm_step(&regular,&r,t);
    for(unsigned long long t=1075000;t<=2200000;t+=75000)tm_step(&jitter,&r,t);
    assert(regular.angle==48&&jitter.angle==48);
    for(int i=0;i<10000;++i) {
        r.gear=i%3==0?TM_PARK:i%3==1?TM_REVERSE:TM_DRIVE;r.open=(unsigned int)i%64;
        advance(&s,&r,1);assert(s.camera.value>=0&&s.camera.value<=48*1024);
        for(int p=0;p<6;++p)assert(s.panels[p]>=0&&s.panels[p]<=15);
    }
    puts("Tesla Model: transitions, reversals, all panel masks, disconnect and pause passed");
}
