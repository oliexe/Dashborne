#include "scene.h"
#define FRAME 1024
static void settle(struct tm_motion *m,int frame) {
    *m=(struct tm_motion){.value=frame*FRAME,.from=frame*FRAME,.target=frame*FRAME};
}
static int animate(struct tm_motion *m,int target,unsigned int dt,unsigned int frame_us) {
    target*=FRAME;
    if(m->target!=target) {
        m->from=m->value;m->target=target;m->elapsed=0;
        unsigned int distance=(unsigned int)(target>m->from?target-m->from:m->from-target);
        m->duration=distance*frame_us/FRAME;
    }
    if(m->value!=m->target) {
        if(dt>=m->duration-m->elapsed){m->value=m->target;m->elapsed=m->duration;}
        else {
            m->elapsed+=dt;
            long long t=(long long)m->elapsed*FRAME/m->duration;
            /* Smoothstep: stationary endpoints, no overshoot on reversal. */
            long long ease=t*t*(3*FRAME-2*t)/(FRAME*FRAME);
            m->value=m->from+(int)((m->target-m->from)*ease/FRAME);
        }
    }
    /* Do not declare a panel closed or a camera settled before it reaches its
       endpoint: rotation and lamp placement depend on these exact frames. */
    if(m->value==m->target)return m->target/FRAME;
    return m->target>m->from?m->value/FRAME:(m->value+FRAME-1)/FRAME;
}
int tm_step(struct tm_scene *s,const struct tm_reading *input,unsigned long long now) {
    struct tm_reading r=*input;
    if(!r.connected)r=(struct tm_reading){0};
    if(r.gear<TM_PARK || r.gear>TM_DRIVE)r.gear=TM_UNKNOWN;
    if(r.speed<0 || r.speed>400)r.speed=0;
    r.known&=63;r.open&=r.known;
    if(r.connected && s->initialized)r.open|=s->data.open & ~r.known;
    int changed=!s->initialized || s->data.connected!=r.connected ||
        s->data.left!=r.left || s->data.right!=r.right ||
        s->data.lights_known!=r.lights_known || s->data.low_beam!=r.low_beam || s->data.high_beam!=r.high_beam;
    int target=r.gear==TM_DRIVE?48:(r.gear==TM_REVERSE || r.gear==TM_NEUTRAL)?24:0;
    /* Open panels take priority over the driving camera. Never hide an open
       door just because the gear changed. Unknown panel bytes stay unknown. */
    if(r.open)target=0;
    if(!s->initialized) {
        s->initialized=1;s->tick=now;s->road_clock=now;
        for(int p=0;p<TM_PANEL_COUNT;++p) {
            s->panels[p]=(r.open>>p&1)?15:0;settle(&s->panel_motion[p],s->panels[p]);
        }
        s->angle=target;settle(&s->camera,target);
    }
    unsigned long long dt=now>=s->tick?now-s->tick:0;
    if(now<s->tick || dt>250000){s->tick=now;s->road_clock=now;dt=0;}
    int steps=(int)(dt/25000);
    if(steps) {
        s->tick+=(unsigned long long)steps*25000;
        int old=s->angle;
        /* Finish closing panels before rotating away from the parked body. */
        int still_open=0;for(int p=0;p<TM_PANEL_COUNT;++p)still_open|=s->panels[p];
        s->angle=animate(&s->camera,still_open?0:target,(unsigned int)steps*25000,25000);
        changed|=old!=s->angle;
        for(int p=0;p<TM_PANEL_COUNT;++p) {
            old=s->panels[p];
            int goal=(r.open>>p&1)?15:0;
            if(!s->angle)s->panels[p]=animate(&s->panel_motion[p],goal,(unsigned int)steps*25000,30000);
            changed|=old!=s->panels[p];
        }
    }
    /* Keep the road phase still during camera motion. This also avoids decoding
       a new road PNG and a new camera PNG in the same animation update. */
    int camera_ready=s->camera.value==target*FRAME;
    if(r.connected && r.speed && !r.open && camera_ready && (r.gear==TM_DRIVE || r.gear==TM_REVERSE)) {
        unsigned long long interval=(unsigned long long)(65000-(r.speed>100?100:r.speed)*400);
        unsigned long long elapsed=now>=s->road_clock?now-s->road_clock:0;
        if(elapsed>250000){s->road_clock=now;elapsed=0;}
        int advance=(int)(elapsed/interval);
        if(advance) {
            s->road=(s->road+(r.gear==TM_REVERSE?-advance:advance)+40)%20;
            s->road_clock+=(unsigned long long)advance*interval;changed=1;
        }
    } else s->road_clock=now;
    if(s->data.gear!=r.gear || s->data.speed!=r.speed || s->data.open!=r.open)changed=1;
    s->data=r;return changed;
}
static void add(struct tm_layer *out,int *n,const char *name,int frame,int x,int y,int w,int h) {
    if(*n<TM_LAYERS)out[(*n)++]=(struct tm_layer){name,frame,x,y,w,h};
}
int tm_layers(const struct tm_scene *s,struct tm_layer out[TM_LAYERS]) {
    int n=0,parts=0;for(int p=0;p<TM_PANEL_COUNT;++p)parts|=s->panels[p];
    int driving=s->angle>24,overhead=s->angle==24;
    if(s->data.connected && (driving || overhead) && s->data.gear!=TM_NEUTRAL)
        add(out,&n,"road",s->road,105,82,300,300);
    if(s->data.connected && s->angle==48 && s->data.lights_known && (s->data.low_beam || s->data.high_beam))
        add(out,&n,s->data.high_beam?"distance_light":"dipped_light",-1,140,16,237,180);
    if(s->angle>0) {
        if(s->angle<=24)add(out,&n,"P_N",s->angle,14,68,482,322);
        else {
            /* The second sequence continues the same overhead camera. Its
               source has a different canvas, matched here at the seam. */
            int f=s->angle-24,w=145+f*3,h=350+f*7;
            /* Reframe the rear view toward the tile's center, leaving room
               for the road ahead instead of crowding the bottom edge. */
            int lift=(f*60+12)/24;
            add(out,&n,"n_road",f,264-w/2,75-f*4-lift,w,h);
        }
    } else if(!parts)add(out,&n,"P_N",0,14,68,482,322);
    else {
        /* Far-side doors, hollow body, then near-side panels. All six
           sequences run closed→open; compositing preserves their occlusion. */
        add(out,&n,"door_right_back",s->panels[TM_RR],117,134,152,139);
        add(out,&n,"door_right_front",s->panels[TM_FR],32,134,182,160);
        add(out,&n,"naked_car",-1,14,68,482,322);
        add(out,&n,"back_hood",s->panels[TM_TRUNK],285,81,184,134);
        add(out,&n,"door_left_back",s->panels[TM_RL],347,143,165,142);
        add(out,&n,"door_left_front",s->panels[TM_FL],279,146,187,162);
        add(out,&n,"front_hood",s->panels[TM_FRUNK],54,139,220,136);
    }
    return n;
}
int tm_lamps(const struct tm_scene *s,struct tm_lamp out[4]) {
    if(!s->data.connected || (s->angle!=0 && s->angle!=24 && s->angle!=48))return 0;
    int n=0,on=s->angle!=48 && s->data.lights_known && (s->data.low_beam || s->data.high_beam);
    unsigned int white=s->data.high_beam?0xf4f8ffu:0xcddcecu;
    /* Small lamp highlights follow the stationary camera views. */
    const int park[2][4]={{176,273,230,258},{38,253,62,264}};
    const int overhead[2][4]={{211,117,217,99},{315,117,308,99}};
    const int rear[2][4]={{188,272,205,275},{331,272,318,277}};
    const int (*points)[4]=s->angle==48?rear:s->angle?overhead:park;
    for(int side=0;side<2;++side) {
        int blink=side?s->data.right:s->data.left;
        if(on || blink)out[n++]=(struct tm_lamp){points[side][0],points[side][1],points[side][2],points[side][3],blink?0xffa52du:white};
    }
    return n;
}
