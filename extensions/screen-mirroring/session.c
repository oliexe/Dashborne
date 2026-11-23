#include "session.h"
static void hold(struct mirror_session *s,int value) {
    if(s->held!=value) {s->ops->hold(value);s->held=value;}
}
int session_close(struct mirror_session *s) {
    if(!s->ops)return 0;
    if(s->state!=MIRROR_CLOSED)s->ops->hide();
    s->displayed=0;s->play_requested=0;hold(s,0);
    if(s->changed) {
        if(s->ops->mode(0)) {s->state=MIRROR_ERROR;return -1;}
        s->changed=0;
        /* Last request wins in the official queue, including rapid Back. */
        s->ops->restart_stock();
    }
    s->state=MIRROR_CLOSED;
    return 0;
}
int session_open(struct mirror_session *s) {
    if(s->state!=MIRROR_CLOSED && session_close(s))return -1;
    if(s->ops->manual_off())return -1;
    s->previous=s->ops->video().generation;s->displayed=0;
    /* A failed patch can still require cleanup (cache/protection failure). */
    s->changed=1;
    if(s->ops->mode(1)) {s->state=MIRROR_ERROR;session_close(s);return -1;}
    hold(s,1);s->ops->hide();
    if(!s->ops->enable()) {s->state=MIRROR_ERROR;session_close(s);return -1;}
    s->state=MIRROR_WAITING;
    s->ops->restart_mirror();
    return 0;
}
void session_tick(struct mirror_session *s) {
    if(s->state==MIRROR_CLOSED || s->state==MIRROR_ERROR)return;
    if(s->ops->manual_off()) {session_close(s);return;}
    int type=s->ops->type();
    if(type!=10)s->play_requested=0;
    else if(!s->play_requested) {s->ops->play();s->play_requested=1;}
    struct video_status v=s->ops->video();
    if(type!=10 || !v.ready || !v.generation || v.generation==s->previous ||
       v.width!=1888 || v.height!=384) {
        if(s->displayed)s->ops->hide();
        s->displayed=0;s->state=MIRROR_WAITING;return;
    }
    if(s->displayed!=v.generation && !s->ops->show(v.generation))return;
    s->displayed=v.generation;s->state=MIRROR_PLAYING;
}
