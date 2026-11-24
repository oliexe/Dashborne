#include <assert.h>
#include "../window.c"
static int opens,closes,started,stopped,clock_ticks,close_pending,selected_protocol;
static int ids[]={VIDEO,WAITING,RETRY_PANEL,STATUS,ASPECT,HINT,RATIO,SELECTOR,AIRPLAY,MIRACAST,CHOICE_STATUS,TITLE,HELP,ASPECT_PANEL};
#define COUNT (int)(sizeof(ids)/sizeof(ids[0]))
static int visibility[COUNT],position_calls;
static struct mirror_rect last_position;
static const char *last_label;
int session_open(struct mirror_session *s) {++started;s->state=MIRROR_WAITING;return 0;}
int session_close(struct mirror_session *s) {
    ++stopped;if(close_pending) {s->state=MIRROR_RESTORING;s->changed=1;return 1;}
    s->state=MIRROR_CLOSED;s->changed=0;return 0;
}
void session_tick(struct mirror_session *s) {++clock_ticks;s->displayed_aspect=s->aspect;
    if(s->state==MIRROR_RESTORING && !close_pending) {s->state=MIRROR_CLOSED;s->changed=0;}}
void session_next_aspect(struct mirror_session *s) {s->aspect=(s->aspect+1)%MIRROR_ASPECT_COUNT;}
static void *get_context(void) {return (void *)123;}
static void open_fake(void *c,const char *name,void *intent) {assert(c==(void *)123 && name && !intent);++opens;}
static void close_fake(void *c,const char *name) {assert(c==(void *)123 && name);++closes;}
static int id_fake(void *p) {return *(int *)p;}
static void *find_fake(void *s,int id) {(void)s;for(int i=0;i<COUNT;++i)if(ids[i]==id)return &ids[i];return 0;}
static void visible_fake(void *p,int n) {visibility[(int *)p-ids]=n;}
static void text_fake(void *p,const char *t) {assert(t);if(*(int *)p==RATIO)last_label=t;}
static void position_fake(void *p,const struct mirror_rect *r) {
    assert(*(int *)p==VIDEO);last_position=*r;++position_calls;
}
static void selected_fake(void *p,int value) {if(value)selected_protocol=*(int *)p;}
static void noop(void *p) {(void)p;}
static void unreg(void *p,int id) {(void)p;assert(id==TIMER);}
static void reg(void *p,int id,int ms) {(void)p;assert(id==TIMER && ms==100);}
static void logger(const struct dashborne_host *h,const char *t) {(void)h;assert(t);}
int main(void) {
    struct dashborne_host host={.log=logger};struct mirror_session state={.supported=MIRROR_SUPPORT(MIRROR_AIRPLAY)|MIRROR_SUPPORT(MIRROR_MIRACAST)};
    api=&host;session=&state;context=get_context;open_activity=open_fake;close_activity=close_fake;
    id_of=id_fake;find=find_fake;visible=visible_fake;text=text_fake;position=position_fake;selected=selected_fake;
    ctor=noop;dtor=noop;base_created=noop;register_timer=reg;unregister_timer=unreg;
    window_open();window_open();assert(opens==1 && pending);
    void *s=create();assert(s && live==s && !create());created(s);resumed(s);
    assert(started==0 && selecting && visibility[7] && selected_protocol==AIRPLAY);
    int choice=MIRACAST;click(s,&choice);assert(state.protocol==MIRROR_MIRACAST && selected_protocol==MIRACAST);
    state.supported=MIRROR_SUPPORT(MIRROR_AIRPLAY);choice=AIRPLAY;click(s,&choice);
    choice=MIRACAST;click(s,&choice);assert(state.protocol==MIRROR_AIRPLAY); /* Unsupported stays disabled. */
    choice=START;click(s,&choice);
    assert(started==1 && !selecting && !visibility[7] && !visibility[0] && visibility[1]);
    state.state=MIRROR_PLAYING;timer_secondary((char *)s+4,TIMER);
    assert(visibility[0] && !visibility[1] && !visibility[2] && clock_ticks==1);
    assert(last_position.width==1920 && last_position.height==480 && position_calls==1 && !visibility[5]);
    int aspect_button=ASPECT;click(s,&aspect_button);
    assert(last_position.width==854 && position_calls==2 && started==1 && !closes);
    assert(last_label==aspect_label(1) && visibility[0] && visibility[5]);
    for(int i=0;i<20;++i)timer(s,TIMER);
    assert(!visibility[5]); /* Ratio hint never becomes a permanent video bar. */
    timer(s,TIMER);assert(position_calls==2); /* Do not repaint unchanged geometry every tick. */
    int button=BACK;close_pending=1;click(s,&button);assert(!closes && exit_requested && !closing);
    int count=started;choice=RETRY;click(s,&choice);assert(started==count);
    timer(s,TIMER);assert(!closes && !closing);
    close_pending=0;timer(s,TIMER);assert(closes==1 && closing);
    click(s,&button);assert(closes==1);
    int ticks=clock_ticks;timer(s,TIMER);click(s,&aspect_button);
    assert(clock_ticks==ticks && state.aspect==1); /* Closing screen cannot restart work. */
    paused(s);assert(state.state==MIRROR_CLOSED);
    assert(window_cleanup()==-1); /* Live native callbacks cannot be unloaded. */
    destroy(s);assert(!live && !pending && stopped>0);
    assert(!window_cleanup());
    return 0;
}
