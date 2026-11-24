#include <assert.h>
#include <stdio.h>
#include <string.h>
#define fopen test_fopen
#define fgets test_fgets
#define fclose test_fclose
#define mprotect test_mprotect
#include "../extension.c"
#undef fopen
#undef fgets
#undef fclose
#undef mprotect

static _Alignas(4096) void *native_activity[512];
static int available=1,attached,registered,unregistered,clicks,destructs,pauses,resumes;
static int live_bitmaps,bitmap_peak,painted,invalidations,protect_fail,edit_mode;
static unsigned long long now=1000000;
static struct readings input={.connected=1,.gear=1,.body_known=63};
static char status_value[100],button_value[32];
static const struct dashborne_host api={.extension_directory="/test/vehicle-widget"};
int source_bind(const struct dashborne_host *h){(void)h;return 0;}
unsigned long long source_microseconds(void){return now;}
void vehicle_read(struct readings *r){*r=input;}
int registry_bind(const struct dashborne_host *h){(void)h;return 0;}
int registry_cleanup(void){return 0;}
int registry_widget(const void *w){(void)w;return available;}
int views_bind(const struct dashborne_host *h,void (*paint)(void *)){(void)h;assert(paint==draw);return 0;}
int views_create(void *p){assert(p==(void *)1);attached=1;return 0;}
int views_sync(void){return available;}
int views_editing(void){return edit_mode;}
void views_invalidate(void){++invalidations;}
void views_label(const char *s){snprintf(status_value,sizeof(status_value),"%s",s);}
void views_preview(int value){snprintf(button_value,sizeof(button_value),"%s",value?"Stop":"Preview");}
int views_click(void *c){return c==(void *)4 && !edit_mode;}
int views_detach(void){attached=0;return 0;}
int views_cleanup(void){return views_detach();}
void *test_fopen(const char *name,const char *mode){(void)name;(void)mode;return (void *)1;}
char *test_fgets(char *buffer,int length,void *file){(void)file;snprintf(buffer,(usize)length,"%lx-%lx r--p 0000",(unsigned long)native_activity,(unsigned long)native_activity+4096);return buffer;}
int test_fclose(void *file){(void)file;return 0;}
int test_mprotect(void *page,usize length,int protection){assert(page==native_activity && length==4096);(void)protection;return protect_fail?-1:0;}
static void original_created(void *p){assert(p==(void *)1);}
static void original_click(void *p,void *c){(void)p;(void)c;++clicks;}
static int original_timer(void *p,int id){(void)p;assert(id!=TIMER);return 7;}
static void original_pause(void *p){(void)p;++pauses;}
static void original_resume(void *p){(void)p;++resumes;}
static void original_destroy(void *p){assert(p==(void *)1);assert(!live && !live_bitmaps && !attached);++destructs;}
static void log_message(const struct dashborne_host *h,const char *s){(void)h;assert(strstr(s,"vehicle-widget"));}
static void register_tick(void *p,int id,int ms){assert(p==(void *)1 && id==TIMER && ms==16);++registered;}
static void unregister_tick(void *p,int id){assert(p==(void *)1 && id==TIMER);++unregistered;}
static void init_bitmap(void *p){((void **)p)[0]=(void *)1;++live_bitmaps;if(live_bitmaps>bitmap_peak)bitmap_peak=live_bitmaps;}
static void free_bitmap(void *p){assert(((void **)p)[0]==(void *)1);((void **)p)[0]=0;--live_bitmaps;}
static int read_bitmap(void *p,const char *path,int a,int b){assert(p && strstr(path,"/assets/motion/") && !a && !b);return 1;}
static void paint_bitmap(void *p,void *surface,int x,int y,int w,int h){assert(p && surface==(void *)3 && x>=0 && y>=0 && x+w<=480 && y+h<=288);++painted;}
static void tick(unsigned long long delta){now+=delta;assert(timer((void *)1,TIMER)==1);if(shown)draw((void *)3);}
int main(void) {
    struct dashborne_host h=api;h.log=log_message;host=&h;
    stock_create=original_created;stock_click=original_click;stock_timer=stock_timer_secondary=original_timer;
    stock_pause=original_pause;stock_resume=original_resume;stock_destroy=stock_delete=original_destroy;
    register_timer=register_tick;unregister_timer=unregister_tick;
    bitmap_ctor=init_bitmap;bitmap_dtor=free_bitmap;bitmap_load=read_bitmap;bitmap_fill=paint_bitmap;
    created((void *)1);assert(live==(void *)1 && registered==1 && attached);
    tick(1);assert(shown && live_bitmaps==65 && painted && !strcmp(status_value,"Live: Park"));
    assert(timer((void *)1,1)==7 && timer_secondary((void *)5,1)==7);
    int writes=invalidations;for(int i=0;i<100;++i)tick(16667);assert(invalidations==writes);
    clicked((void *)1,(void *)5);assert(clicks==1);
    for(int i=0;i<30;++i) {
        clicked((void *)1,(void *)4);assert(preview);
        tick(2500000);clicked((void *)1,(void *)4);assert(!preview && !strcmp(button_value,"Preview"));
        tick(1200000);assert(scene.frame==0 && !failed);
    }
    input.body_open=63;tick(100000);tick(500000);assert(scene.body==63 && bitmap_peak<=77);
    input.connected=0;tick(100000);assert(!scene.body && scene.frame==0 && !strcmp(status_value,"Waiting for vehicle"));
    edit_mode=1;tick(100000);assert(shown && editing && attached);
    clicked((void *)1,(void *)4);assert(!preview);
    edit_mode=0;available=0;tick(100000);assert(!shown && !live_bitmaps);
    available=3;tick(100000);assert(shown==3 && live_bitmaps==65 && !editing);
    /* Adjacent pages reuse the same cache, including during slide callbacks. */
    for(int i=0;i<100;++i){available=i%3+1;tick(16667);assert(live_bitmaps==65 && attached);}
    pause_view((void *)1);assert(paused && !live_bitmaps && pauses==1);
    tick(100000);assert(attached && !live_bitmaps);
    resume_view((void *)1);tick(100000);assert(!paused && resumes==1 && live_bitmaps);
    destroy((void *)1);assert(destructs==1 && !live_bitmaps && !live);
    writes=invalidations;tick(100000);assert(invalidations==writes);
    /* A conflicting hook must keep the library resident; restoration can be retried. */
    activity_table=native_activity;native_activity[4]=(void *)original_click;hook_count=0;
    assert(!add_hook(4,(void *)original_click,(void *)clicked,0));assert(!map_table());
    installed=1;native_activity[4]=(void *)clicked;protect_fail=1;assert(deactivate()==-1 && installed);
    protect_fail=0;native_activity[4]=(void *)original_timer;assert(deactivate()==-1 && installed);
    native_activity[4]=(void *)clicked;assert(!deactivate() && !installed && !host);
    assert(native_activity[4]==(void *)original_click);
    puts("Vehicle widget adapter: native injection, placement, preview, lifecycle and hook cleanup passed");
}
