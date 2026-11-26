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
    assert(!tm_step(&s,&r,++now));
    r.gear=TM_DRIVE;r.speed=30;advance(&s,&r,24);assert(s.angle==24);
    advance(&s,&r,24);assert(s.angle==48);
    int old=s.road;advance(&s,&r,1);advance(&s,&r,1);assert(s.road!=old);
    r.gear=TM_REVERSE;advance(&s,&r,24);assert(s.angle==24);
    r.speed=0;advance(&s,&r,1);old=s.road;advance(&s,&r,100);assert(s.road==old);
    /* Interrupt a transition with an open door. Camera returns before the
       panel moves, and the driving camera cannot hide an unclosed door. */
    r.speed=30;r.gear=TM_DRIVE;advance(&s,&r,16);advance(&s,&r,10);assert(s.angle>24&&s.angle<48);
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
    r.gear=TM_DRIVE;r.speed=30;r.open=0;r.left=r.right=1;r.lights_known=r.high_beam=1;
    s=(struct tm_scene){0};tm_step(&s,&r,now);int count=tm_layers(&s,layers);assert(count==2);
    struct tm_lamp lamps[4];assert(tm_lamps(&s,lamps)==2&&lamps[0].y0==282);
    s.angle=0;assert(tm_lamps(&s,lamps)==3&&lamps[0].color==0xffa52d);
    s.data.left=s.data.right=0;assert(tm_lamps(&s,lamps)==3&&lamps[0].color==0xf4f4f4);
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
        r.gear=i%3==0?TM_PARK:i%3==1?TM_REVERSE:TM_DRIVE;r.open=(unsigned int)i%64;r.speed=i%5?0:50;
        advance(&s,&r,1);assert(s.camera.value>=0&&s.camera.value<=48*1024);
        assert(s.approach.value>=-16*1024&&s.approach.value<=16*1024);
        for(int p=0;p<6;++p)assert(s.panels[p]>=0&&s.panels[p]<=15);
    }
    /* Direction cues wait for a settled camera and stop at the first movement. */
    for(int direction=0;direction<2;++direction) {
        s=(struct tm_scene){0};r=(struct tm_reading){.connected=1,.known=63,
            .gear=direction?TM_REVERSE:TM_DRIVE};
        tm_step(&s,&r,now);assert(s.shift==(direction?-1:1));
        count=tm_layers(&s,layers);assert(count==3);
        assert(!strcmp(layers[count-1].name,direction?"night_back_gears_remind":"night_forward_gears_remind"));
        assert(!strcmp(layers[0].name,"n_road")&&layers[0].frame==(direction?8:0));
        assert(layers[0].w==180&&layers[0].h==436);
        assert(direction?layers[0].y<0:layers[0].y+layers[0].h>458);
        int start_y=layers[count-1].y;
        advance(&s,&r,28);tm_layers(&s,layers);
        assert(layers[count-1].y==start_y+(direction?95:-95));
        assert(layers[count-1].y>=0&&layers[count-1].y+layers[count-1].h<=458);
        advance(&s,&r,28);tm_layers(&s,layers);assert(layers[count-1].y==start_y);
        unsigned int phase=s.shift_phase;now+=90000000;tm_step(&s,&r,now);assert(s.shift_phase==phase);
        r.speed=1;tm_step(&s,&r,now);assert(!s.shift&&tm_layers(&s,layers)==1);
        old=s.approach.value;int road=s.road;
        advance(&s,&r,1);assert(s.angle==24&&s.approach.value!=old&&s.road==road);
        advance(&s,&r,15);assert(s.angle==24&&!s.approach.value);
        advance(&s,&r,48);assert(!s.approach.value&&tm_layers(&s,layers)==2);
        r.speed=0;advance(&s,&r,64);assert(s.shift);
        r.gear=direction?TM_DRIVE:TM_REVERSE;tm_step(&s,&r,now);assert(!s.shift);
        advance(&s,&r,31);assert(!s.shift);advance(&s,&r,1);assert(s.shift==(direction?1:-1));
        r.open=1;tm_step(&s,&r,now);assert(!s.shift);
        advance(&s,&r,100);r.open=0;advance(&s,&r,17);assert(!s.shift);
        advance(&s,&r,100);assert(s.shift);
        r.gear=TM_NEUTRAL;tm_step(&s,&r,now);assert(!s.shift);
        r.gear=TM_PARK;advance(&s,&r,100);assert(!s.shift);
        r.gear=TM_DRIVE;advance(&s,&r,100);assert(s.shift);
        r.connected=0;tm_step(&s,&r,now);assert(!s.shift);
    }
    /* Advance smooth zoom on every 16 ms callback, with no 25 ms staircase. */
    s=(struct tm_scene){0};r=(struct tm_reading){.connected=1,.known=63,.gear=TM_DRIVE};
    tm_step(&s,&r,1000000);r.speed=10;
    int previous=s.approach.value;
    for(unsigned long long t=1016000;t<=1400000;t+=16000) {
        tm_step(&s,&r,t);assert(s.approach.value<previous);previous=s.approach.value;
    }
    assert(!s.approach.value);
    /* Elapsed time, not callback count, determines the geometric pose. */
    regular=jitter=(struct tm_scene){0};r.speed=0;
    tm_step(&regular,&r,2000000);tm_step(&jitter,&r,2000000);r.speed=10;
    for(unsigned long long t=2016000;t<=2192000;t+=16000)tm_step(&regular,&r,t);
    for(unsigned long long t=2048000;t<=2192000;t+=48000)tm_step(&jitter,&r,t);
    assert(regular.approach.value==jitter.approach.value);
    /* The arrow eases into each turn instead of reversing at full speed. */
    s=(struct tm_scene){0};r.speed=0;tm_step(&s,&r,3000000);
    tm_layers(&s,layers);int y0=layers[2].y;
    tm_step(&s,&r,3050000);tm_layers(&s,layers);assert(y0-layers[2].y<=2);
    tm_step(&s,&r,3300000);tm_step(&s,&r,3350000);tm_layers(&s,layers);int ym=layers[2].y;
    tm_step(&s,&r,3400000);tm_layers(&s,layers);assert(ym-layers[2].y>=9);
    /* Crossing the overhead/close-up seam keeps the same image and center. */
    s=(struct tm_scene){.angle=24,.camera={.value=24*1024}};
    count=tm_layers(&s,layers);assert(count==1&&!strcmp(layers[0].name,"n_road")&&layers[0].frame==0);
    struct tm_layer overhead=layers[0];s.approach.value=1;
    assert(tm_layers(&s,layers)==1&&layers[0].x==overhead.x&&layers[0].y==overhead.y&&layers[0].w==overhead.w&&layers[0].h==overhead.h);
    s.approach.value=0;s.camera.value=24*1024+512;
    tm_layers(&s,layers);assert(layers[0].frame==0&&layers[0].w<=overhead.w&&layers[0].y<=overhead.y);
    /* Moving roads advance on every 60 Hz tick, even at walking speed. */
    s=(struct tm_scene){0};r=(struct tm_reading){.connected=1,.gear=TM_DRIVE,.speed=1};
    tm_step(&s,&r,1000000);
    for(unsigned long long t=1016667;t<2000000;t+=16667) {
        unsigned int previous_phase=s.road_phase;
        assert(tm_step(&s,&r,t));assert(s.road_phase!=previous_phase);
    }
    /* Side warnings alone never illuminate the model. Rear anchors follow the
       source frame through every reverse zoom, not a fixed screen position. */
    r.detected_left=r.detected_right=1;r.left=r.right=0;
    tm_step(&s,&r,2016667);assert(!tm_lamps(&s,lamps));
    for(int zoom=0;zoom<=16;++zoom) {
        s=(struct tm_scene){.angle=24,.camera={.value=24*1024},
            .approach={.value=-zoom*1024},.data={.connected=1,.gear=TM_REVERSE,.left=1}};
        count=tm_layers(&s,layers);int lamp_count=tm_lamps(&s,lamps);assert(count>0&&lamp_count>0);
        struct tm_layer body=layers[0];
        assert(lamps[0].x0>=body.x && lamps[0].x1<=body.x+body.w);
        assert(lamps[0].y0>=body.y && lamps[0].y1<=body.y+body.h);
    }
    puts("Tesla Model: transitions, reversals, all panel masks, disconnect and pause passed");
}
