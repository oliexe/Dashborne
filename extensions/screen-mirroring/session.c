#include "session.h"
void session_next_aspect(struct mirror_session *s) {
    s->aspect=(aspect_index(s->aspect)+1)%MIRROR_ASPECT_COUNT;
}
static void hold(struct mirror_session *s,int value) {
    if(s->held!=value) {s->ops->hold(value);s->held=value;}
}
static int finish_close(struct mirror_session *s) {
    if(s->network_owned) {
        if(s->ops->network_mode()!=1)return 1;
        s->network_owned=0;s->restore_requested=0;
    }
    if(s->changed) {
        s->ops->restart_stock();s->changed=0;
    }
    s->state=s->error?MIRROR_ERROR:MIRROR_CLOSED;
    return 0;
}
/* 0: restored, 1: waiting for hotspot, -1: retryable restoration failure. */
int session_close(struct mirror_session *s) {
    if(!s->ops)return 0;
    if(s->state!=MIRROR_CLOSED)s->ops->hide();
    s->displayed=0;s->play_requested=0;hold(s,0);
    if(s->changed && s->ops->mode(MIRROR_STOCK)) {
        s->error=MIRROR_RESTORE_FAILED;s->state=MIRROR_ERROR;return -1;
    }
    if(s->network_owned && !s->restore_requested) {
        /* Queue AP even if get_mode still says AP: a P2P request may be pending. */
        s->ops->network(1);s->restore_requested=1;s->wait_ticks=0;
        s->state=MIRROR_RESTORING;return 1;
    }
    return finish_close(s);
}
static void start_receiver(struct mirror_session *s) {
    s->previous=s->ops->video().generation;
    s->ops->restart_mirror();s->state=MIRROR_WAITING;
}
int session_open(struct mirror_session *s) {
    if(!protocol_valid(s->protocol) || !(s->supported&MIRROR_SUPPORT(s->protocol)))return -1;
    if(s->state!=MIRROR_CLOSED || s->changed || s->network_owned) {
        int result=session_close(s);if(result)return result;
    }
    s->error=MIRROR_NO_ERROR;
    if(s->ops->manual_off())return -1;
    s->displayed=0;s->play_requested=0;s->wait_ticks=0;
    /* A failed patch can still require cleanup (cache/protection failure). */
    s->changed=1;
    if(s->ops->mode(s->protocol)) {
        s->error=MIRROR_START_FAILED;session_close(s);return -1;
    }
    hold(s,1);s->ops->hide();
    if(!s->ops->enable()) {
        s->error=MIRROR_START_FAILED;session_close(s);return -1;
    }
    if(s->protocol==MIRROR_MIRACAST) {
        s->network_owned=1;s->ops->network(3);s->state=MIRROR_STARTING;
    } else start_receiver(s);
    return 0;
}
void session_tick(struct mirror_session *s) {
    if(s->state==MIRROR_RESTORING) {
        if(!finish_close(s))return;
        if(++s->wait_ticks>=MIRROR_NETWORK_TICKS) {
            s->error=MIRROR_RESTORE_FAILED;s->state=MIRROR_ERROR;
            s->restore_requested=0; /* Back/Retry can submit a new AP request. */
        }
        return;
    }
    if(s->state==MIRROR_CLOSED || s->state==MIRROR_ERROR)return;
    if(s->ops->manual_off()) {session_close(s);return;}
    if(s->state==MIRROR_STARTING) {
        if(s->ops->network_mode()==3) {start_receiver(s);return;}
        if(++s->wait_ticks>=MIRROR_NETWORK_TICKS) {
            s->error=MIRROR_NETWORK_FAILED;session_close(s);
        }
        return;
    }
    int type=s->ops->type(),expected=protocol_link_type(s->protocol);
    if(type!=expected)s->play_requested=0;
    else if(!s->play_requested) {s->ops->play();s->play_requested=1;}
    struct video_status v=s->ops->video();
    /* Miracast negotiates its own size. Keep the known AirPlay geometry guard. */
    int geometry=s->protocol==MIRROR_AIRPLAY?(v.width==1888 && v.height==384):
        (v.width>0 && v.height>0 && v.width<=4096 && v.height<=4096);
    if(type!=expected || !v.ready || !v.generation || v.generation==s->previous || !geometry) {
        if(s->displayed)s->ops->hide();
        s->displayed=0;s->state=MIRROR_WAITING;return;
    }
    if(s->displayed!=v.generation || s->displayed_aspect!=s->aspect) {
        if(!s->ops->show(v.generation,s->aspect))return;
        s->displayed_aspect=s->aspect;
    }
    s->displayed=v.generation;s->state=MIRROR_PLAYING;
}
