#include "scene.h"
#define FRAME 1024
static int shift_offset(unsigned int phase) {
    unsigned int leg=phase<=700000?phase:1400000-phase;
    long long t=(long long)leg*FRAME/700000;
    return (int)((95*t*t*(3*FRAME-2*t)+FRAME*FRAME*FRAME/2)/(FRAME*FRAME*FRAME));
}
static int shift_direction(const struct tm_scene *s,const struct tm_reading *r) {
    if(!r->connected || r->speed || r->open)return 0;
    for(int p=0;p<TM_PANEL_COUNT;++p)if(s->panels[p])return 0;
    if(r->gear==TM_DRIVE && s->camera.value==24*FRAME && s->approach.value==16*FRAME)return 1;
    if(r->gear==TM_REVERSE && s->camera.value==24*FRAME && s->approach.value==-16*FRAME)return -1;
    return 0;
}
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
        s->data.detected_left!=r.detected_left || s->data.detected_right!=r.detected_right ||
        s->data.lights_known!=r.lights_known || s->data.low_beam!=r.low_beam || s->data.high_beam!=r.high_beam;
    int target=r.gear==TM_DRIVE?(r.speed?48:24):(r.gear==TM_REVERSE || r.gear==TM_NEUTRAL)?24:0;
    /* Open panels take priority over the driving camera. Never hide an open
       door just because the gear changed. Unknown panel bytes stay unknown. */
    if(r.open)target=0;
    if(!s->initialized) {
        s->initialized=1;s->tick=now;s->road_clock=now;
        for(int p=0;p<TM_PANEL_COUNT;++p) {
            s->panels[p]=(r.open>>p&1)?15:0;settle(&s->panel_motion[p],s->panels[p]);
        }
        s->angle=target;settle(&s->camera,target);
        int closeup=r.connected&&!r.speed&&!r.open?(r.gear==TM_DRIVE?16:r.gear==TM_REVERSE?-16:0):0;
        settle(&s->approach,closeup);
    }
    unsigned long long dt=now>=s->tick?now-s->tick:0;
    if(now<s->tick || dt>250000){s->tick=now;s->road_clock=now;dt=0;}
    if(dt) {
        s->tick=now;
        int old=s->angle,old_camera=s->camera.value;
        /* Finish closing panels before rotating away from the parked body. */
        int still_open=0;for(int p=0;p<TM_PANEL_COUNT;++p)still_open|=s->panels[p];
        s->angle=animate(&s->camera,s->approach.value?24:still_open?0:target,(unsigned int)dt,25000);
        changed|=old!=s->angle || ((s->camera.value>24*FRAME || old_camera>24*FRAME) && old_camera!=s->camera.value);
        int closeup=r.connected&&!r.speed&&!r.open&&!still_open&&s->camera.value==24*FRAME?
            (r.gear==TM_DRIVE?16:r.gear==TM_REVERSE?-16:0):0;
        old=s->approach.value;animate(&s->approach,closeup,(unsigned int)dt,25000);
        changed|=old!=s->approach.value;
        for(int p=0;p<TM_PANEL_COUNT;++p) {
            old=s->panels[p];
            int goal=(r.open>>p&1)?15:0;
            if(!s->angle)s->panels[p]=animate(&s->panel_motion[p],goal,(unsigned int)dt,30000);
            changed|=old!=s->panels[p];
        }
    }
    /* Keep the road phase still during camera motion. This also avoids decoding
       a new road PNG and a new camera PNG in the same animation update. */
    int camera_ready=s->camera.value==target*FRAME;
    if(r.connected && r.speed && !r.open && !s->approach.value && camera_ready && (r.gear==TM_DRIVE || r.gear==TM_REVERSE)) {
        /* Integrate distance at every display tick. No quantized PNG loop. */
        unsigned int advance=(unsigned int)(dt*(unsigned int)r.speed/18);
        if(advance) {
            s->road_phase=(s->road_phase+(r.gear==TM_REVERSE?1000000-advance%1000000:advance))%1000000;
            s->road=(int)(s->road_phase/50000);changed=1;
        }
        s->road_clock=now;
    } else s->road_clock=now;
    int shift=shift_direction(s,&r);
    if(shift!=s->shift){s->shift=shift;s->shift_phase=0;changed=1;}
    else if(shift && dt) {
        int before=shift_offset(s->shift_phase);
        s->shift_phase=(s->shift_phase+(unsigned int)dt)%1400000;
        changed|=before!=shift_offset(s->shift_phase);
    }
    if(s->data.gear!=r.gear || s->data.speed!=r.speed || s->data.open!=r.open)changed=1;
    s->data=r;return changed;
}
static void add(struct tm_layer *out,int *n,const char *name,int frame,int x,int y,int w,int h) {
    if(*n<TM_LAYERS)out[(*n)++]=(struct tm_layer){name,frame,x,y,w,h};
}
int tm_layers(const struct tm_scene *s,struct tm_layer out[TM_LAYERS]) {
    int n=0,parts=0;for(int p=0;p<TM_PANEL_COUNT;++p)parts|=s->panels[p];
    if(s->approach.value) {
        /* The launch view crops the overhead body at the bottom in Drive and
           at the top in Reverse. Interpolate its position and size before
           resuming the regular camera sequence. */
        int amount=s->approach.value,forward=amount>0;if(amount<0)amount=-amount;
        int w=145+35*amount/(16*FRAME),h=350+86*amount/(16*FRAME);
        int y=75+((forward?300:-220)-75)*amount/(16*FRAME);
        int center=264-9*amount/(16*FRAME);
        add(out,&n,"n_road",forward?0:8*amount/(16*FRAME),center-w/2,y,w,h);
        if(s->shift) {
            int offset=shift_offset(s->shift_phase);
            add(out,&n,forward?"forward_gears_rectangle":"back_gears_rectangle",-1,145,forward?57:95,220,313);
            add(out,&n,forward?"night_forward_gears_remind":"night_back_gears_remind",-1,216,forward?191-offset:95+offset,78,181);
        }
        return n;
    }
    int driving=s->angle>24,overhead=s->angle==24;
    if(s->data.connected && (driving || overhead) && s->data.gear!=TM_NEUTRAL)
        add(out,&n,"road",s->road,75,55,360,380);
    if(s->angle>0) {
        if(s->angle<24)add(out,&n,"P_N",s->angle,14,68,482,322);
        else if(s->angle==24 && s->camera.value<=24*FRAME)add(out,&n,"n_road",0,192,75,145,350);
        else {
            /* The second sequence continues the same overhead camera. Its
               source has a different canvas, matched here at the seam. */
            int progress=s->camera.value-24*FRAME;
            int f=progress/FRAME;
            int w=145-progress*13/(24*FRAME),h=350-progress*42/(24*FRAME);
            int x=264-w/2-progress*9/(24*FRAME);
            int y=75;
            add(out,&n,"n_road",f,x,y,w,h);
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
static void lamp(struct tm_lamp *out,int *n,const struct tm_layer *body,int sw,int sh,
                 int x0,int y0,int x1,int y1,unsigned int color) {
    int ay=body->y+y0*body->h/sh,by=body->y+y1*body->h/sh;
    if((ay<0 && by<0) || (ay>=458 && by>=458) || *n>=4)return;
    out[(*n)++]=(struct tm_lamp){body->x+x0*body->w/sw,ay,
        body->x+x1*body->w/sw,by,color};
}
int tm_lamps(const struct tm_scene *s,struct tm_lamp out[4]) {
    if(!s->data.connected)return 0;
    struct tm_layer layers[TM_LAYERS],body={"P_N",0,14,68,482,322};
    int count=tm_layers(s,layers),rear=0;
    for(int i=0;i<count;++i) {
        if(layers[i].name[0]=='n' && layers[i].name[1]=='_'){body=layers[i];rear=1;break;}
        if(layers[i].name[0]=='P'){body=layers[i];break;}
    }
    int n=0,on=s->data.lights_known && (s->data.low_beam || s->data.high_beam);
    unsigned int white=0xf4f4f4,amber=0xffa52d;
    if(!rear) {
        if(s->angle)return 0; /* Park rotation has no verified lamp anchors. */
        if(s->data.left || on) {
            unsigned int c=s->data.left?amber:white;
            lamp(out,&n,&body,482,322,147,204,200,199,c);
            lamp(out,&n,&body,482,322,200,199,216,185,c);
        }
        if(s->data.right || on)lamp(out,&n,&body,482,322,20,188,37,194,s->data.right?amber:white);
        return n;
    }
    /* Anchors measured in the actual pack's source pixels, before all camera
       translation/scaling. All five color variants share this geometry. */
    static const unsigned short tail_y[25]={290,299,306,312,316,318,319,318,316,312,308,302,296,290,283,276,269,263,257,252,248,245,243,242,243};
    int f=body.frame;if(f<0 || f>24)return 0;
    int sw=149,sh=361,y=tail_y[f];
    int outer=f?20:25,inner=f?40:36;
    if(s->data.left)lamp(out,&n,&body,sw,sh,outer,y,inner,y+5,amber);
    if(s->data.right)lamp(out,&n,&body,sw,sh,sw-outer-2,y,sw-inner-2,y+5,amber);
    if(!f) {
        if(s->data.left || on)lamp(out,&n,&body,sw,sh,20,30,28,17,s->data.left?amber:white);
        if(s->data.right || on)lamp(out,&n,&body,sw,sh,129,30,121,17,s->data.right?amber:white);
    }
    return n;
}
