#include <assert.h>
#include "../session.h"
static int airplay,enabled=1,off,type,held,restore_fail,apply_fail,show_fail;
static int mirror_requests,stock_requests,shown,hidden,play_requests,last_shown_aspect;
static struct video_status video;
static int mode(int value) {
    if(value) {airplay=1;return apply_fail;}
    if(restore_fail)return -1;
    airplay=0;return 0;
}
static int enable(void) {return enabled;}
static int get_off(void) {return off;}
static int get_type(void) {return type;}
static void restart_mirror(void) {assert(airplay);++mirror_requests;}
static void restart_stock(void) {assert(!airplay);++stock_requests;}
static void hide(void) {++hidden;}
static void hold(int value) {held=value;}
static int show(unsigned int generation,int aspect) {
    assert(generation==video.generation);++shown;
    if(show_fail)return 0;
    last_shown_aspect=aspect;return 1;
}
static struct video_status get_video(void) {return video;}
static void play(void) {assert(type==10);++play_requests;}
static const struct session_ops ops={.mode=mode,.enable=enable,.manual_off=get_off,.type=get_type,
    .restart_mirror=restart_mirror,.restart_stock=restart_stock,.hide=hide,.hold=hold,
    .show=show,.video=get_video,.play=play};
int main(void) {
    struct mirror_session s={.ops=&ops,.protocol=MIRROR_AIRPLAY,.supported=MIRROR_SUPPORT(MIRROR_AIRPLAY)};
    assert(!airplay && !held && !mirror_requests);
    video=(struct video_status){.generation=7,.ready=1,.width=1888,.height=384};
    assert(!session_open(&s) && airplay && held && mirror_requests==1);
    type=10;session_tick(&s);assert(s.state==MIRROR_WAITING && !shown); /* Old AirPlay frame. */
    video.generation=8;type=2;session_tick(&s);assert(!shown); /* CarPlay shutdown race. */
    type=10;video.width=1290;session_tick(&s);assert(!shown); /* Old widget geometry. */
    video.width=1888;video.ready=0;session_tick(&s);assert(!shown);
    video.ready=1;show_fail=1;session_tick(&s);assert(s.state==MIRROR_WAITING);
    show_fail=0;session_tick(&s);assert(s.state==MIRROR_PLAYING && shown==2);
    session_tick(&s);assert(shown==2 && play_requests==2); /* No repeated play or layer repositioning. */
    /* Live aspect changes reuse the current decoder generation, never the restart queue. */
    for(int i=1;i<=MIRROR_ASPECT_COUNT;++i) {
        session_next_aspect(&s);session_tick(&s);
        assert(s.state==MIRROR_PLAYING && s.displayed==8);
        assert(last_shown_aspect==i%MIRROR_ASPECT_COUNT && s.displayed_aspect==s.aspect);
        assert(mirror_requests==1 && stock_requests==0 && play_requests==2);
        int count=shown;session_tick(&s);assert(shown==count);
    }
    session_next_aspect(&s);show_fail=1;session_tick(&s);
    assert(s.aspect==1 && s.displayed_aspect==0 && s.state==MIRROR_PLAYING);
    show_fail=0;session_tick(&s);assert(s.displayed_aspect==1); /* Retry failed resize. */
    video.ready=0;session_tick(&s);assert(s.state==MIRROR_WAITING && !s.displayed);
    session_next_aspect(&s);session_tick(&s);assert(s.aspect==2 && s.state==MIRROR_WAITING);
    video.ready=1;video.generation=9;session_tick(&s);
    assert(s.state==MIRROR_PLAYING && s.displayed_aspect==2);
    assert(!session_close(&s) && !airplay && !held && stock_requests==1);
    assert(!session_close(&s) && stock_requests==1);
    assert(!session_open(&s) && s.aspect==2);assert(!session_close(&s)); /* Keep preset on Retry/reopen. */
    assert(stock_requests==2 && mirror_requests==2);
    assert(!session_open(&s));off=1;session_tick(&s);
    assert(!airplay && !held && s.state==MIRROR_CLOSED);
    assert(session_open(&s)==-1 && mirror_requests==3);off=0;
    assert(!session_open(&s));restore_fail=1;
    assert(session_close(&s)==-1 && airplay && s.changed && s.state==MIRROR_ERROR);
    int before=mirror_requests;
    assert(session_open(&s)==-1 && mirror_requests==before); /* Cannot stack over failed restore. */
    restore_fail=0;assert(!session_close(&s) && !airplay && !held);
    apply_fail=1;assert(session_open(&s)==-1 && !airplay && !held && !s.changed);
    apply_fail=0;enabled=0;assert(session_open(&s)==-1 && !airplay && !held);
    assert(hidden>0);
    const int widths[]={1920,854,640,270,222};
    for(int i=0;i<MIRROR_ASPECT_COUNT;++i) {
        struct mirror_rect r=aspect_rect(i),p=aspect_physical_rect(i);
        assert(r.width==widths[i] && r.height==480 && r.top==0);
        assert(r.left*2+r.width==1920 && !(r.width%2));
        assert(p.left==0 && p.top==r.left && p.width==480 && p.height==r.width);
        assert(p.left+p.width<=480 && p.top+p.height<=1920);
        assert(aspect_label(i));
    }
    assert(aspect_rect(-1).width==1920 && aspect_rect(99).width==1920);
    return 0;
}
