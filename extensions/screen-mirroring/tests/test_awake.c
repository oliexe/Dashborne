#include <assert.h>
static int protect_calls, fail_protect;
static int fake_mprotect(void *p, __SIZE_TYPE__ length, int flags) {
    (void)p; (void)length; (void)flags;
    return ++protect_calls==fail_protect?-1:0;
}
#define mprotect fake_mprotect
#include "../awake.c"
#undef mprotect
static unsigned char display[12];
static int type, shown, off, changes, timer_calls, pause_calls, resume_calls;
static int clear_video_in_timer;
static void *expected_self;
static void logger(const struct dashborne_host *h,const char *s) {(void)h;(void)s;}
static struct dashborne_host host={.log=logger};
static void set_inhibit(void *p,unsigned int mask,int enable) {
    assert(p==display && mask==0x80);
    if(enable)display[8]|=mask; else display[8]&=~mask;
    ++changes;
}
static int get_type(void) {return type;}
static int get_shown(void) {return shown;}
static int get_off(void *p) {assert(p==display);return off;}
static int saved_timer(void *self,int id) {
    assert(self==expected_self && id==42);
    ++timer_calls;
    if(clear_video_in_timer)shown=0;
    return 17;
}
static void saved_pause(void *self) {assert(self==expected_self);++pause_calls;}
static void saved_resume(void *self) {assert(self==expected_self);++resume_calls;}
int main(void) {
    char activity[32]; expected_self=activity;
    api=&host; controller=display;
    inhibit=set_inhibit; link_type=get_type; video_shown=get_shown; user_off=get_off;
    original_timer=saved_timer; original_pause=saved_pause; original_resume=saved_resume;
    display[8]=5; /* Other official owners must survive all transitions. */
    type=10; shown=1;
    assert(timer(activity,42)==17 && display[8]==5); /* Not resumed yet. */
    resume(activity);
    assert(resume_calls==1 && display[8]==0x85 && changes==1);
    assert(timer_secondary(activity+4,42)==17 && timer_calls==2 && changes==1);
    off=1; timer(activity,42);
    assert(display[8]==5 && !held); /* Explicit screen-off wins. */
    off=0; timer(activity,42); assert(held);
    type=2; timer(activity,42); assert(!held && display[8]==5); /* CarPlay. */
    type=10; shown=0; timer(activity,42); assert(!held);
    shown=1; timer(activity,42); assert(held);
    type=0; timer(activity,42); assert(!held); /* Disconnect. */
    type=10; timer(activity,42); assert(held);
    clear_video_in_timer=1; timer(activity,42); assert(!held);
    clear_video_in_timer=0; shown=1; timer(activity,42); assert(held);
    pause(activity); assert(pause_calls==1 && !held && display[8]==5);
    timer(activity,42); assert(!held); /* Stray paused timers cannot reacquire. */
    resume(activity); assert(held);
    assert(awake_restore()==0 && !held && !api && display[8]==5);

    /* Failed permission restoration must keep the extension loaded for retry. */
    void *vtable[25]={0};
    table=vtable; page=vtable; protection=1; installed=1; api=&host;
    for(unsigned int i=0;i<HOOK_COUNT;++i) {
        hooks[i].saved=(void *)saved_timer;
        table[hooks[i].slot]=hooks[i].replacement;
    }
    protect_calls=0; fail_protect=1;
    assert(awake_restore()==-1 && installed);
    for(unsigned int i=0;i<HOOK_COUNT;++i)assert(table[hooks[i].slot]==hooks[i].replacement);
    protect_calls=0; fail_protect=2;
    assert(awake_restore()==-1 && installed);
    for(unsigned int i=0;i<HOOK_COUNT;++i)assert(table[hooks[i].slot]==hooks[i].saved);
    protect_calls=0; fail_protect=0;
    assert(awake_restore()==0 && !installed && !api);
    assert(awake_restore()==0);

    table=vtable; page=vtable; installed=1; api=&host;
    vtable[7]=(void *)saved_pause; /* A later hook must not be overwritten. */
    assert(awake_restore()==-1 && installed);
    vtable[7]=hooks[0].replacement;
    assert(awake_restore()==0);
    return 0;
}
