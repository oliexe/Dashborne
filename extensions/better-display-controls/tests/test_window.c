#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../window.c"
struct display_settings display_settings;
int display_minute=-1,display_active=-1,display_dirty;
const char *display_status="Stock display controls";
static int changes,saves,save_ok=1,closes,timers;static void *bound[4];
struct control {int id;};static struct control controls[32];static int count;
static void *lookup(void *s,int id){(void)s;for(int i=0;i<count;i++)if(controls[i].id==id)return controls+i;assert(count<32);controls[count].id=id;return controls+count++;}
static int identity(void *p){return ((struct control *)p)->id;}
static void set_text(void *p,const char *s){assert(p&&s);}
static void selection(void *p,int v){assert(p);(void)v;}
static void seek(void *p,int n){seek_changed(&listener,p,n);}
static void bind_seek(void *p,void *l){int id=identity(p),i=(id>=919540?2:0)+(id%10==1);bound[i]=l;}
static void nothing(void *s){(void)s;}
static void reg(void *s,int id,int ms){(void)s;assert(id==TIMER&&ms==1000);timers++;}
static void unreg(void *s,int id){(void)s;assert(id==TIMER);timers--;}
static void *ctx(void){return controls;}
static void close_page(void *s,const char *name){assert(s==controls&&!strcmp(name,ACTIVITY));closes++;}
void display_tick(void){}
void display_changed(void){display_dirty=1;changes++;}
int display_save(void){saves++;if(save_ok)display_dirty=0;return save_ok;}
int main(void){
 display_defaults(&display_settings);find=lookup;id_of=identity;text=set_text;selected=selection;progress=seek;seek_listener=bind_seek;
 base_created=nothing;dtor=nothing;add_timer=reg;remove_timer=unreg;context=ctx;close_activity=close_page;
 live=controls;created(live);assert(timers==1&&!changes);
 assert(sizeof(listener_table)/sizeof(*listener_table)==5);
 for(int i=0;i<4;i++)assert(bound[i]==&listener);
 /* The native runtime calls both tracking callbacks around progress updates. */
 void **v=*(void ***)&listener;
 ((void (*)(void *,void *))v[3])(&listener,lookup(live,919530));
 ((void (*)(void *,void *,int))v[2])(&listener,lookup(live,919530),0);
 ((void (*)(void *,void *))v[4])(&listener,lookup(live,919530));
 assert(display_settings.day.brightness==1&&changes==1);
 seek_changed(&listener,lookup(live,919540),254);assert(display_settings.night.brightness==255);
 seek_changed(&listener,lookup(live,919541),30);assert(display_settings.night.black==30);
 int n=changes;seek_changed(&listener,lookup(live,919541),101);assert(changes==n);
 click(live,lookup(live,299521));assert(display_settings.mode==1);
 click(live,lookup(live,299520));assert(display_settings.enabled);
 click(live,lookup(live,299523));assert(!display_settings.enabled);
 assert(display_window_cleanup()==-1);save_ok=0;back(live);assert(!closes&&!closing&&saves==1);
 save_ok=1;back(live);assert(closes==1&&closing&&!display_dirty);
 destroy_only(live);assert(!live&&timers==0&&!display_window_cleanup());
 pending=1;assert(display_window_cleanup()==-1);pending=0;
 puts("display window: complete slider listener ABI, render guard, profile edits, save retry and unload ownership passed");
}
