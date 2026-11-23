#include <assert.h>
#include "../window.c"
static int opens,closes,started,stopped,clock_ticks;
static int ids[]={VIDEO,WAITING,RETRY,STATUS};
static int visibility[4];
int session_open(struct mirror_session *s) {++started;s->state=MIRROR_WAITING;return 0;}
int session_close(struct mirror_session *s) {++stopped;s->state=MIRROR_CLOSED;return 0;}
void session_tick(struct mirror_session *s) {(void)s;++clock_ticks;}
static void *get_context(void) {return (void *)123;}
static void open_fake(void *c,const char *name,void *intent) {assert(c==(void *)123 && name && !intent);++opens;}
static void close_fake(void *c,const char *name) {assert(c==(void *)123 && name);++closes;}
static int id_fake(void *p) {return *(int *)p;}
static void *find_fake(void *s,int id) {(void)s;for(int i=0;i<4;++i)if(ids[i]==id)return &ids[i];return 0;}
static void visible_fake(void *p,int n) {visibility[(int *)p-ids]=n;}
static void text_fake(void *p,const char *t) {(void)p;assert(t);}
static void noop(void *p) {(void)p;}
static void unreg(void *p,int id) {(void)p;assert(id==TIMER);}
static void reg(void *p,int id,int ms) {(void)p;assert(id==TIMER && ms==100);}
static void logger(const struct dashborne_host *h,const char *t) {(void)h;assert(t);}
int main(void) {
    struct dashborne_host host={.log=logger};struct mirror_session state={0};
    api=&host;session=&state;context=get_context;open_activity=open_fake;close_activity=close_fake;
    id_of=id_fake;find=find_fake;visible=visible_fake;text=text_fake;
    ctor=noop;dtor=noop;base_created=noop;register_timer=reg;unregister_timer=unreg;
    window_open();window_open();assert(opens==1 && pending);
    void *s=create();assert(s && live==s && !create());created(s);resumed(s);
    assert(started==1 && !visibility[0] && visibility[1]);
    state.state=MIRROR_PLAYING;timer_secondary((char *)s+4,TIMER);
    assert(visibility[0] && !visibility[1] && !visibility[2] && clock_ticks==1);
    int button=BACK;click(s,&button);click(s,&button);assert(closes==1 && closing);
    timer(s,TIMER);assert(clock_ticks==1); /* Closing screen cannot restart work. */
    paused(s);assert(state.state==MIRROR_CLOSED);
    assert(window_cleanup()==-1); /* Live native callbacks cannot be unloaded. */
    destroy(s);assert(!live && !pending && stopped>0);
    assert(!window_cleanup());
    return 0;
}
