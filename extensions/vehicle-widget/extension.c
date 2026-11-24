#include "dashborne_extension.h"
#include "scene.h"
#include "data.h"
#include "registry.h"
#include "views.h"
#include "../tesla-ui/sample.h"
typedef __SIZE_TYPE__ usize;
typedef __UINTPTR_TYPE__ uintptr;
extern int snprintf(char *,usize,const char *,...);
extern int strcmp(const char *,const char *);
extern usize strlen(const char *);
extern void *memcpy(void *,const void *,usize);
extern int mprotect(void *,usize,int);
extern void *fopen(const char *,const char *);
extern char *fgets(char *,int,void *);
extern int fclose(void *),sscanf(const char *,const char *,...);

#define TIMER 97400
#define TABLE_WORDS 63
struct bitmap {void *words[6];int initialized;};
struct hook {int index;void *before,*after;};
static const struct dashborne_host *host;
static void *live,*drawing;
static void **activity_table;
static void *hook_page;
static int hook_protection,installed,hook_count,paused,preview,failed,shown,editing;
static unsigned long long started,next_sample;
static struct hook hooks[24];
static struct vehicle_scene scene;
static struct bitmap bitmaps[VEHICLE_BITMAP_SLOTS];
static void (*stock_create)(void *),(*stock_pause)(void *),(*stock_resume)(void *);
static void (*stock_destroy)(void *),(*stock_delete)(void *),(*stock_click)(void *,void *);
static int (*stock_timer)(void *,int),(*stock_timer_secondary)(void *,int);
static void (*register_timer)(void *,int,int),(*unregister_timer)(void *,int);
static void (*bitmap_ctor)(void *),(*bitmap_dtor)(void *);
static int (*bitmap_load)(void *,const char *,int,int);
static void (*bitmap_fill)(void *,void *,int,int,int,int);

static void image_unload(void *context,int slot) {
    (void)context;
    if(bitmaps[slot].initialized)bitmap_dtor(bitmaps[slot].words);
    bitmaps[slot]=(struct bitmap){0};
}
static int image_load(void *context,int slot,const char *name) {
    (void)context;char path[512];
    int n=snprintf(path,sizeof(path),"%s/assets/motion/%s",host->extension_directory,name);
    if(n<0 || (usize)n>=sizeof(path))return 0;
    bitmap_ctor(bitmaps[slot].words);bitmaps[slot].initialized=1;
    return bitmap_load(bitmaps[slot].words,path,0,0);
}
static void image_invalidate(void *context) {(void)context;views_invalidate();}
static void image_draw(void *context,int slot,int x,int y,int w,int h) {
    (void)context;
    if(drawing)bitmap_fill(bitmaps[slot].words,drawing,x*4/5,y*4/5,w*4/5,h*4/5);
}
static const struct vehicle_scene_ops scene_ops={image_load,image_unload,image_invalidate,image_draw};
static void draw(void *surface) {
    if(!paused && scene.active) {drawing=surface;vehicle_scene_draw(&scene);drawing=0;}
}
static int detach(void) {
    if(views_detach())return -1;
    vehicle_scene_close(&scene);drawing=0;preview=0;shown=0;return 0;
}
static void update(unsigned long long now) {
    if(!live || paused || failed)return;
    shown=views_sync();editing=views_editing();
    if(editing && preview){preview=0;views_preview(0);next_sample=0;}
    if(!shown) {vehicle_scene_close(&scene);return;}
    if(!scene.active && vehicle_scene_open(&scene,&scene_ops,0)) {
        failed=1;views_label("Car images unavailable");host->log(host,"vehicle-widget: asset load failed");return;
    }
    if(!next_sample || now>=next_sample || next_sample-now>100000) {
        next_sample=now+100000;struct readings r;const char *sample=0;
        if(preview)sample=sample_read(&r,now>=started?now-started:0);
        else vehicle_read(&r);
        vehicle_scene_update(&scene,r.connected && !r.demo,r.gear,r.body_known,r.body_open,now);
        char line[100];
        if(sample)snprintf(line,sizeof(line),"Preview: %s",sample);
        else if(!scene.connected)snprintf(line,sizeof(line),"Waiting for vehicle");
        else if(!scene.panels_known)snprintf(line,sizeof(line),"Panel status unavailable");
        else snprintf(line,sizeof(line),"Live: %s",r.gear==1?"Park":r.gear==2?"Reverse":r.gear==3?"Neutral":"Drive");
        views_label(scene.body_failed?"Car image unavailable":line);
    }
    vehicle_scene_tick(&scene,now);
}
static void created(void *self) {
    stock_create(self);
    if(live)return;
    live=self;paused=preview=failed=editing=0;next_sample=0;
    if(views_create(self)){failed=1;detach();host->log(host,"vehicle-widget: native controls unavailable");return;}
    register_timer(self,TIMER,16);host->log(host,"vehicle-widget: car views attached to native page containers");
}
static int timer(void *self,int id) {
    if(id!=TIMER)return stock_timer(self,id);
    if(self==live)update(source_microseconds());
    return 1;
}
static int timer_secondary(void *self,int id) {
    if(id!=TIMER)return stock_timer_secondary(self,id);
    return timer((unsigned char *)self-sizeof(void *),id);
}
static void clicked(void *self,void *control) {
    if(self==live && !paused && !failed && !editing && shown && views_click(control)) {
        preview=!preview;started=source_microseconds();next_sample=0;
        views_preview(preview);update(started);return;
    }
    stock_click(self,control);
}
static void pause_view(void *self) {
    if(self==live){paused=1;unregister_timer(self,TIMER);vehicle_scene_close(&scene);preview=0;views_preview(0);}
    stock_pause(self);
}
static void resume_view(void *self) {
    stock_resume(self);
    if(self==live && !failed){paused=0;next_sample=0;register_timer(self,TIMER,16);}
}
static void retiring(void *self) {
    if(self==live){paused=1;unregister_timer(self,TIMER);detach();live=0;}
}
static void destroy(void *self) {retiring(self);stock_destroy(self);}
static void deleting(void *self) {retiring(self);stock_delete(self);}
static void destroy_secondary(void *self) {
    __PTRDIFF_TYPE__ offset=(__PTRDIFF_TYPE__)(*(void ***)self)[-2];destroy((unsigned char *)self+offset);
}
static void delete_secondary(void *self) {
    __PTRDIFF_TYPE__ offset=(__PTRDIFF_TYPE__)(*(void ***)self)[-2];deleting((unsigned char *)self+offset);
}
static int add_hook(int index,void *expected,void *replacement,int chained) {
    if(!expected || hook_count>=24 ||
       (activity_table[index]!=expected && (!chained || !host->owns_callback || !host->owns_callback(host,activity_table[index]))))return -1;
    hooks[hook_count++]=(struct hook){index,activity_table[index],replacement};return 0;
}
static int map_table(void) {
    uintptr address=(uintptr)activity_table&~(uintptr)4095;
    if((uintptr)(activity_table+TABLE_WORDS)>address+4096)return -1;
    void *file=fopen("/proc/self/maps","r");if(!file)return -1;
    char line[512],perms[8];unsigned long start,end;int found=0;
    while(fgets(line,sizeof(line),file))if(sscanf(line,"%lx-%lx %4s",&start,&end,perms)==3 && address>=start && address+4096<=end) {
        if(perms[0]=='r' && perms[3]=='p'){hook_page=(void *)address;hook_protection=1|(perms[1]=='w'?2:0)|(perms[2]=='x'?4:0);found=1;}break;
    }
    fclose(file);return found?0:-1;
}
static int activate(const struct dashborne_host *api) {
    if(!api || api->abi_version!=DASHBORNE_EXTENSION_ABI || api->struct_size<sizeof(*api) || !api->resolve || !api->log || host)return -1;
    host=api;hook_count=0;
    if(source_bind(api))return -1;
#define BIND(v,handle,s) do {*(void **)(&(v))=api->resolve(handle,s);if(!(v)){api->log(api,"vehicle-widget: missing " s);return -1;}}while(0)
#define UI(v,s) BIND(v,api->ui_runtime,s)
#define APP(v,s) BIND(v,api->official_library,s)
    APP(activity_table,"_ZTV16dash1actActivity");
    APP(stock_create,"_ZN16dash1actActivity8onCreateEv");APP(stock_timer,"_ZN16dash1actActivity7onTimerEi");
    APP(stock_timer_secondary,"_ZThn4_N16dash1actActivity7onTimerEi");
    APP(stock_pause,"_ZN16dash1actActivity7onPauseEv");APP(stock_resume,"_ZN16dash1actActivity8onResumeEv");
    APP(stock_destroy,"_ZN16dash1actActivityD1Ev");APP(stock_delete,"_ZN16dash1actActivityD0Ev");
    APP(stock_click,"_ZN16dash1actActivity7onClickEP6ZKBase");
    UI(register_timer,"_ZN7BaseApp13registerTimerEii");UI(unregister_timer,"_ZN7BaseApp15unregisterTimerEi");
    UI(bitmap_ctor,"_ZN13BitmapWrapperC1Ev");UI(bitmap_dtor,"_ZN13BitmapWrapperD1Ev");
    UI(bitmap_load,"_ZN13BitmapWrapper18loadBitmapFromFileEPKcii");
    UI(bitmap_fill,"_ZN13BitmapWrapper17fillBoxWithBitmapEPviiii");
#undef APP
#undef UI
#undef BIND
    if(registry_bind(api) || views_bind(api,draw))return -1;
    if(add_hook(2,(void *)stock_destroy,(void *)destroy,0) || add_hook(3,(void *)stock_delete,(void *)deleting,0) ||
       add_hook(4,(void *)stock_click,(void *)clicked,1) || add_hook(6,(void *)stock_create,(void *)created,0) ||
       add_hook(7,(void *)stock_timer,(void *)timer,0) || add_hook(9,(void *)stock_resume,(void *)resume_view,0) ||
       add_hook(10,(void *)stock_pause,(void *)pause_view,0) || add_hook(24,(void *)stock_timer_secondary,(void *)timer_secondary,0))return -1;
    *(void **)(&stock_click)=hooks[2].before;
    const int offsets[]={4,24,28,32,36,40,44,48};
    for(int n=0;n<8;++n)for(int deleting_flag=0;deleting_flag<2;++deleting_flag) {
        char symbol[80];snprintf(symbol,sizeof(symbol),"_ZThn%d_N16dash1actActivityD%dEv",offsets[n],deleting_flag?0:1);
        void *expected=api->resolve(api->official_library,symbol);int found=0;
        if(!expected)return -1;
        for(int i=20;i<TABLE_WORDS;++i)if(activity_table[i]==expected) {
            if(add_hook(i,expected,deleting_flag?(void *)delete_secondary:(void *)destroy_secondary,0))return -1;
            ++found;
        }
        if(found!=1)return -1;
    }
    if(map_table() || mprotect(hook_page,4096,hook_protection|2))return -1;
    for(int i=0;i<hook_count;++i)activity_table[hooks[i].index]=hooks[i].after;
    installed=1;
    if(mprotect(hook_page,4096,hook_protection))return -1;
    api->log(api,"vehicle-widget: native child views ready");return 0;
}
static int deactivate(void) {
    /* Native widget pools and editor state remain live until Activity teardown. */
    if(live)return -1;
    if(live)unregister_timer(live,TIMER);
    paused=1;
    if(detach())return -1;
    if(installed) {
        for(int i=0;i<hook_count;++i)if(activity_table[hooks[i].index]!=hooks[i].after && activity_table[hooks[i].index]!=hooks[i].before)return -1;
        if(mprotect(hook_page,4096,hook_protection|2))return -1;
        for(int i=0;i<hook_count;++i)activity_table[hooks[i].index]=hooks[i].before;
        if(mprotect(hook_page,4096,hook_protection))return -1;
    }
    if(views_cleanup() || registry_cleanup())return -1;
    installed=0;live=0;host=0;hook_count=0;failed=0;return 0;
}
DASHBORNE_EXTENSION_EXPORT const struct dashborne_extension *dashborne_extension_v1(void) {
    static const struct dashborne_extension extension={DASHBORNE_EXTENSION_ABI,sizeof(extension),"vehicle-widget",activate,deactivate};
    return &extension;
}
