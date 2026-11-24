#include "motion.h"
/* Reconstructed one-second orbit; easing is baked into the rendered frames.
   Reversal starts at the currently displayed frame, without an endpoint jump. */
static float sample(struct camera_motion *m,unsigned long long now) {
    if(now<m->start)return m->target;
    float t=(float)(now-m->start)/1066667.0f;
    if(t>=1)return m->target;
    return m->from+(m->target-m->from)*t;
}
void camera_target(struct camera_motion *m,int drive,unsigned long long now,int immediate) {
    float target=drive?1:0;
    if(!m->primed || immediate) {
        m->position=m->from=m->target=target;m->start=now;m->primed=1;
    } else if(target!=m->target) {
        m->position=sample(m,now);m->from=m->position;m->target=target;m->start=now;
    }
}
int camera_frame(struct camera_motion *m,unsigned long long now) {
    m->position=sample(m,now);
    int frame=(int)(m->position*64+.5f);return frame<0?0:frame>64?64:frame;
}
