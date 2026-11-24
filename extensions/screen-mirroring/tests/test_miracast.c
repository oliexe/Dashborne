#include <assert.h>
#include "../session.h"
static int applied,net_mode=1,net_request,network_requests,held,mirror_requests,stock_requests,plays,shown,hidden,restore_fail;
static struct video_status video={.generation=1,.ready=1,.width=1888,.height=384};
static int type=10;
static int mode(int p) {if(!p && restore_fail)return -1;applied=p;return 0;}
static int yes(void) {return 1;}
static int no(void) {return 0;}
static int get_type(void) {return type;}
static void mirror(void) {assert(applied==MIRROR_MIRACAST && net_mode==3);++mirror_requests;}
static void stock(void) {assert(!applied && net_mode==1);++stock_requests;}
static void hide(void) {++hidden;}
static void hold(int value) {held=value;}
static int show(unsigned int generation,int aspect) {(void)aspect;assert(type==11 && generation==video.generation);++shown;return 1;}
static struct video_status get_video(void) {return video;}
static void play(void) {assert(type==11);++plays;}
static void network(int mode) {net_request=mode;++network_requests;}
static int network_mode(void) {return net_mode;}
static const struct session_ops ops={mode,yes,no,get_type,mirror,stock,hide,hold,show,get_video,play,network,network_mode};
static struct mirror_session fresh(void) {
    return (struct mirror_session){.ops=&ops,.protocol=MIRROR_MIRACAST,.supported=MIRROR_SUPPORT(MIRROR_MIRACAST)};
}
int main(void) {
    struct mirror_session s=fresh();
    s.supported=0;assert(session_open(&s)==-1 && !applied && !network_requests);s=fresh();
    assert(!session_open(&s) && applied==MIRROR_MIRACAST && held && net_request==3);
    session_tick(&s);assert(s.state==MIRROR_STARTING && !mirror_requests);
    net_mode=3;session_tick(&s);assert(mirror_requests==1 && s.state==MIRROR_WAITING);
    type=11;session_tick(&s);assert(!shown); /* Generation before forced restart. */
    video.generation=2;type=10;session_tick(&s);assert(!shown); /* AirPlay never leaks into Miracast. */
    type=11;video.width=0;session_tick(&s);assert(!shown);
    video.width=1920;video.height=1080;session_tick(&s);assert(shown==1 && s.state==MIRROR_PLAYING);
    session_tick(&s);assert(shown==1); /* No repeated presentation. */
    video.ready=0;session_tick(&s);assert(s.state==MIRROR_WAITING);
    video.ready=1;video.generation=3;video.width=1280;video.height=720;
    session_tick(&s);assert(shown==2 && s.state==MIRROR_PLAYING);
    int requests=mirror_requests;session_next_aspect(&s);session_tick(&s);
    assert(mirror_requests==requests && shown==3);
    assert(session_close(&s)==1 && !applied && !held && net_request==1 && !stock_requests);
    session_tick(&s);assert(!stock_requests && s.state==MIRROR_RESTORING);
    net_mode=1;session_tick(&s);assert(stock_requests==1 && s.state==MIRROR_CLOSED);
    assert(!session_close(&s) && stock_requests==1);
    /* Back during startup cancels the queued P2P transition, even while AP is still active. */
    assert(!session_open(&s) && net_request==3);
    assert(session_close(&s)==1 && net_request==1 && stock_requests==1);
    session_tick(&s);assert(stock_requests==2 && !s.network_owned && !s.changed);
    /* P2P startup timeout rolls back and releases the screen inhibitor. */
    assert(!session_open(&s));
    for(int i=0;i<MIRROR_NETWORK_TICKS;++i)session_tick(&s);
    assert(s.error==MIRROR_NETWORK_FAILED && !applied && !held && net_request==1);
    session_tick(&s);assert(s.state==MIRROR_ERROR && !s.changed);
    /* Failed AP restoration remains owned and retryable; it never starts stock in P2P mode. */
    assert(!session_open(&s));net_mode=3;session_tick(&s);
    assert(session_close(&s)==1);int before=stock_requests;
    for(int i=0;i<MIRROR_NETWORK_TICKS;++i)session_tick(&s);
    assert(s.state==MIRROR_ERROR && s.error==MIRROR_RESTORE_FAILED && s.network_owned && s.changed);
    assert(stock_requests==before);
    assert(session_close(&s)==1);net_mode=1;session_tick(&s);assert(stock_requests==before+1 && !s.changed);
    /* A patch restore failure cannot start another protocol or release ownership. */
    assert(!session_open(&s));restore_fail=1;
    assert(session_close(&s)==-1 && s.changed && s.network_owned && !held);
    requests=mirror_requests;assert(session_open(&s)==-1 && mirror_requests==requests);
    restore_fail=0;assert(session_close(&s)==1);session_tick(&s);assert(!s.changed && !s.network_owned);
    assert(plays && hidden);
}
