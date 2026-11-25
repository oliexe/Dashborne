#include <assert.h>
#include <stdio.h>
#include <string.h>
#define clock_gettime test_clock_gettime
#define fopen test_fopen
#define fgets test_fgets
#define fclose test_fclose
#define mprotect test_mprotect
#include "../runtime.c"
#undef clock_gettime
#undef fopen
#undef fgets
#undef fclose
#undef mprotect
int test_clock_gettime(int clock,struct timestamp *t){assert(clock==1);*t=(struct timestamp){1,0};return 0;}
const struct dashborne_host *runtime_host;
static _Alignas(4096) void *native_activity[512];
static int attaches,detaches,registered,unregistered,clicks,destructs,pauses,resumes,ticks,view_paused,protect_fail,registry_fail,views_cleaned;
void views_activity(void *p){if(p)++attaches;}
void views_tick(unsigned long long t){assert(t);++ticks;}
void views_pause(int p){view_paused=p;}
int views_click(void *p){return p==(void *)4;}
int views_bind(const struct dashborne_host *h){(void)h;return 0;}
int views_detach(void){++detaches;return 0;}
int views_cleanup(void){++views_cleaned;return 0;}
int registry_bind(const struct dashborne_host *h){(void)h;return 0;}
int registry_cleanup(void){return registry_fail?-1:0;}
unsigned long crc32(unsigned long c,const unsigned char *p,unsigned int n){(void)p;(void)n;return c;}
void *test_fopen(const char *name,const char *mode){(void)name;(void)mode;return (void *)1;}
char *test_fgets(char *buffer,int length,void *file){(void)file;snprintf(buffer,(usize)length,"%lx-%lx r--p 0000",(unsigned long)native_activity,(unsigned long)native_activity+4096);return buffer;}
int test_fclose(void *file){(void)file;return 0;}
int test_mprotect(void *page,usize length,int protection){assert(page==native_activity && length==4096);(void)protection;return protect_fail?-1:0;}
static void original_created(void *p){assert(p==(void *)1 && live==p && attaches==1);}
static void original_click(void *p,void *c){(void)p;(void)c;++clicks;}
static int original_timer(void *p,int id){(void)p;assert(id!=TIMER);return 7;}
static void original_pause(void *p){(void)p;assert(view_paused);++pauses;}
static void original_resume(void *p){(void)p;++resumes;}
static void original_destroy(void *p){assert(p==(void *)1 && !live && detaches);++destructs;}
static int display_destroyed,display_deleted;
static void display_destroy(void *p){++display_destroyed;original_destroy(p);}
static void display_delete(void *p){++display_deleted;original_destroy(p);}
static int owns(const struct dashborne_host *h,const void *p){(void)h;return p==(void *)original_timer || p==(void *)original_created || p==(void *)display_destroy || p==(void *)display_delete;}
static void log_message(const struct dashborne_host *h,const char *s){(void)h;assert(strstr(s,"widgets:"));}
static void register_tick(void *p,int id,int ms){assert(p==(void *)1 && id==TIMER && ms==16);++registered;}
static void unregister_tick(void *p,int id){assert(p==(void *)1 && id==TIMER);++unregistered;}
int main(void) {
    const struct dashborne_host h={.log=log_message,.owns_callback=owns};host=&h;
    stock_create=original_created;stock_click=original_click;stock_timer=stock_timer_secondary=original_timer;
    stock_pause=original_pause;stock_resume=original_resume;stock_destroy=stock_delete=original_destroy;
    register_timer=register_tick;unregister_timer=unregister_tick;
    created((void *)1);assert(runtime_live() && registered==1);
    assert(timer((void *)1,TIMER)==1 && ticks==1);assert(timer_secondary((void *)(1+sizeof(void *)),TIMER)==1 && ticks==2);
    assert(timer((void *)1,9)==7);clicked((void *)1,(void *)4);assert(!clicks);clicked((void *)1,(void *)5);assert(clicks==1);
    assert(runtime_unbind()==-1 && !views_cleaned);
    pause_view((void *)1);assert(pauses==1 && unregistered==1);resume_view((void *)1);assert(resumes==1 && registered==2 && !view_paused);
    destroy((void *)1);assert(destructs==1 && !runtime_live());
    activity_table=native_activity;hook_count=0;
    native_activity[7]=(void *)original_timer;
    assert(add_hook(7,(void *)timer_secondary,(void *)timer,0)==-1);
    assert(!add_hook(7,(void *)timer_secondary,(void *)timer,1) && hooks[0].before==(void *)original_timer);
    native_activity[24]=(void *)original_timer;
    assert(!add_hook(24,(void *)timer,(void *)timer_secondary,1) && hooks[1].before==(void *)original_timer);
    native_activity[6]=(void *)original_created;
    assert(!add_hook(6,(void *)created,(void *)created,1));
    assert(hooks[2].before==(void *)original_created);
    native_activity[4]=(void *)original_click;hook_count=0;
    assert(!add_hook(4,(void *)original_click,(void *)clicked,0) && !map_table());
    installed=1;native_activity[4]=(void *)clicked;protect_fail=1;assert(runtime_unbind()==-1 && installed);
    protect_fail=0;native_activity[4]=(void *)original_timer;assert(runtime_unbind()==-1 && installed);
    native_activity[4]=(void *)clicked;registry_fail=1;assert(runtime_unbind()==-1 && installed && native_activity[4]==(void *)original_click);
    registry_fail=0;assert(!runtime_unbind() && !installed && !host);
    /* Better Display Controls owns the two primary destructor slots before the
       first widget registers. Both cleanup callbacks must remain in the chain. */
    host=&h;activity_table=native_activity;hook_count=0;
    stock_destroy=stock_delete=original_destroy;
    native_activity[2]=(void *)display_destroy;native_activity[3]=(void *)display_delete;
    native_activity[4]=(void *)stock_click;native_activity[6]=(void *)stock_create;
    native_activity[7]=(void *)stock_timer;native_activity[9]=(void *)stock_resume;
    native_activity[10]=(void *)stock_pause;native_activity[24]=(void *)stock_timer_secondary;
    assert(!prepare_activity_hooks());
    assert(hooks[0].before==(void *)display_destroy && hooks[1].before==(void *)display_delete);
    destroy((void *)1);deleting((void *)1);
    assert(display_destroyed==1 && display_deleted==1);
    for(int i=0;i<hook_count;++i)native_activity[hooks[i].index]=hooks[i].after;
    installed=1;assert(!map_table() && !runtime_unbind());
    assert(native_activity[2]==(void *)display_destroy && native_activity[3]==(void *)display_delete);
    host=&h;hook_count=0;stock_destroy=stock_delete=original_destroy;
    native_activity[2]=(void *)original_click; /* Unowned code is still rejected. */
    assert(prepare_activity_hooks()==-1 && !hook_count);
    puts("Widget runtime: native callback delegation, pause/resume, live-unload refusal, hook conflicts and cleanup retries passed");
}
