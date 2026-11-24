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

struct control {void **vtable;int id;};
static void *native_text[13];
static _Alignas(4096) void *native_activity[512];
static struct control root_control,window_control,car_control,status_control,button_control;
static unsigned char fake_grid[80],fake_widget[80];
static void *widgets[1]={fake_widget};
static void *vector[3]={widgets,widgets+1,widgets+1};
static struct rect parent_position={0,0,1550,480};
static int available=1,parsed,visible_count,registered,unregistered,clicks,destructs,pauses,resumes;
static int live_bitmaps,bitmap_peak,painted,invalidations,protect_fail;
static unsigned long long now=1000000;
static struct readings input={.connected=1,.gear=1,.body_known=63};
static char status_value[100],button_value[32];
static const struct dashborne_host api={.extension_directory="/test/vehicle-widget"};
int source_bind(const struct dashborne_host *h){(void)h;return 0;}
unsigned long long source_microseconds(void){return now;}
void vehicle_read(struct readings *r){*r=input;}
void *test_fopen(const char *name,const char *mode){(void)name;(void)mode;return (void *)1;}
char *test_fgets(char *buffer,int length,void *file){(void)file;snprintf(buffer,(usize)length,"%lx-%lx r--p 0000",(unsigned long)native_activity,(unsigned long)native_activity+4096);return buffer;}
int test_fclose(void *file){(void)file;return 0;}
int test_mprotect(void *page,usize length,int protection){assert(page==native_activity && length==4096);(void)protection;return protect_fail?-1:0;}
static void original_created(void *p){assert(p==(void *)1);}
static void original_click(void *p,void *c){(void)p;(void)c;++clicks;}
static int original_timer(void *p,int id){(void)p;assert(id!=TIMER);return 7;}
static void original_pause(void *p){(void)p;++pauses;}
static void original_resume(void *p){(void)p;++resumes;}
static void original_destroy(void *p){assert(p==(void *)1);assert(!live && !live_bitmaps && car_control.vtable==native_text+2);++destructs;}
static void log_message(const struct dashborne_host *h,const char *s){(void)h;assert(strstr(s,"vehicle-widget"));}
static void *lookup(void *p,int id) {
    assert(p==(void *)1);
    if(id==110001)return &root_control;
    if(!parsed)return 0;
    if(id==WINDOW)return &window_control;
    if(id==CANVAS)return &car_control;
    if(id==STATUS)return &status_control;
    if(id==PREVIEW)return &button_control;
    return 0;
}
static void noop(void *p){(void)p;}
static void json_init(void *p,int type){(void)p;assert(!type);}
static int parse_json(void *p,const char *b,const char *e,void *v,int comments){(void)p;(void)v;assert(e-b==(long)strlen(b) && !comments);return 1;}
static void parse_tree(void *p,const void *v){assert(p==&root_control && v);parsed=1;}
static void listen(void *p,void *l){assert(p==&button_control && (!l || l==(void *)1));}
static void set_visible(void *p,int on){assert(p==&window_control);visible_count=on;}
static void set_text(void *p,const char *s) {
    if(p==&status_control)snprintf(status_value,sizeof(status_value),"%s",s);
    else {assert(p==&button_control);snprintf(button_value,sizeof(button_value),"%s",s);}
}
static void register_tick(void *p,int id,int ms){assert(p==(void *)1 && id==TIMER && ms==16);++registered;}
static void unregister_tick(void *p,int id){assert(p==(void *)1 && id==TIMER);++unregistered;}
static void *get_book(void){return (void *)2;}
static void *get_grid(void *p){assert(p==(void *)2);return fake_grid;}
static void *get_widgets(void *p){assert(p==fake_grid);return vector;}
static void *get_parent(void *p){assert(p==&root_control);return &parent_position;}
static const struct rect *get_position(void *p){assert(p==&parent_position);return &parent_position;}
static unsigned int get_size(void *p,int type){assert(p==fake_grid && type==5);return 0x00020002;}
static int free_cell(void *p,int type,struct cell preferred,struct cell *out,unsigned short excluded) {
    assert(p==fake_grid && type==5 && preferred.x==4 && preferred.y==0 && excluded==65535);
    *out=(struct cell){4,0};return available;
}
static void place(void *p,const struct rect *r){assert(p==&window_control && r->x==1040 && r->y==11 && r->w==510 && r->h==458);}
static int get_id(void *p){return ((struct control *)p)->id;}
static void init_bitmap(void *p){((void **)p)[0]=(void *)1;++live_bitmaps;if(live_bitmaps>bitmap_peak)bitmap_peak=live_bitmaps;}
static void free_bitmap(void *p){assert(((void **)p)[0]==(void *)1);((void **)p)[0]=0;--live_bitmaps;}
static int read_bitmap(void *p,const char *path,int a,int b){assert(p && strstr(path,"/assets/motion/") && !a && !b);return 1;}
static void paint_bitmap(void *p,void *surface,int x,int y,int w,int h){assert(p && surface==(void *)3 && x>=0 && y>=0 && x+w<=480 && y+h<=288);++painted;}
static void native_draw(void *p,void *surface){assert(p==&car_control && surface==(void *)3);}
static void invalid(void *p,const struct rect *r){assert(p==&car_control && !r);++invalidations;}
static void tick(unsigned long long delta){now+=delta;assert(timer((void *)1,TIMER)==1);if(shown)draw(&car_control,(void *)3);}
int main(void) {
    struct dashborne_host h=api;h.log=log_message;host=&h;
    find=lookup;json_ctor=json_init;json_dtor=reader_ctor=reader_dtor=noop;json_parse=parse_json;parse_controls=parse_tree;
    listener=listen;visible=set_visible;text=set_text;position=place;id_of=get_id;
    stock_create=original_created;stock_click=original_click;stock_timer=stock_timer_secondary=original_timer;
    stock_pause=original_pause;stock_resume=original_resume;stock_destroy=stock_delete=original_destroy;
    register_timer=register_tick;unregister_timer=unregister_tick;
    book=get_book;grid_active=get_grid;grid_widgets=get_widgets;grid_size=get_size;grid_free=free_cell;
    parent_of=get_parent;position_of=get_position;*(void **)(fake_grid+40)=&root_control;
    bitmap_ctor=init_bitmap;bitmap_dtor=free_bitmap;bitmap_load=read_bitmap;bitmap_fill=paint_bitmap;
    invalidate=invalid;base_draw=native_draw;text_table=native_text;
    for(int i=0;i<13;++i)draw_table[i]=native_text[i];draw_table[10]=(void *)draw;
    car_control.vtable=native_text+2;button_control.id=PREVIEW;
    created((void *)1);assert(live==(void *)1 && registered==1 && car_control.vtable==draw_table+2);
    tick(1);assert(visible_count && live_bitmaps==65 && painted && !strcmp(status_value,"Live: Park"));
    assert(timer((void *)1,1)==7 && timer_secondary((void *)5,1)==7);
    int writes=invalidations;for(int i=0;i<100;++i)tick(16667);assert(invalidations==writes);
    clicked((void *)1,&root_control);assert(clicks==1);
    for(int i=0;i<30;++i) {
        clicked((void *)1,&button_control);assert(preview);
        tick(2500000);clicked((void *)1,&button_control);assert(!preview && !strcmp(button_value,"Preview"));
        tick(1200000);assert(scene.frame==0 && !failed);
    }
    input.body_open=63;tick(100000);tick(500000);assert(scene.body==63 && bitmap_peak<=77);
    input.connected=0;tick(100000);assert(!scene.body && scene.frame==0 && !strcmp(status_value,"Waiting for vehicle"));
    parent_position.y=20;tick(100000);assert(!visible_count && !live_bitmaps);
    parent_position.y=0;fake_widget[0x14]=1;tick(100000);assert(!visible_count);
    fake_widget[0x14]=0;available=0;tick(100000);assert(!visible_count);
    available=1;tick(100000);assert(visible_count && live_bitmaps);
    pause_view((void *)1);assert(paused && !live_bitmaps && pauses==1);
    tick(100000);assert(!visible_count && !live_bitmaps);
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
