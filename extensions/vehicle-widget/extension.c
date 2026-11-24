#include "dashborne_widget.h"
#include "scene.h"
#include "data.h"
#include "../tesla-ui/sample.h"
typedef __SIZE_TYPE__ usize;
extern int snprintf(char *,usize,const char *,...);
struct bitmap {void *words[6];int initialized;};
static const struct dashborne_host *host;
static const struct dashborne_widgets *widgets;
static struct dashborne_widget_view *instances[36],*painting;
static struct vehicle_scene scene;
static struct bitmap bitmaps[VEHICLE_BITMAP_SLOTS];
static void *drawing;
static int preview,users,failed;
static unsigned long long started,next_sample,last_tick;
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
static void image_invalidate(void *context) {(void)context;for(int i=0;i<36;++i)if(instances[i])widgets->invalidate(instances[i]);}
static void image_draw(void *context,int slot,int x,int y,int w,int h) {
    (void)context;
    if(drawing && painting) {
        int cw=painting->width-24,ch=painting->height-108;
        int scale=cw*1000/600;if(ch*1000/360<scale)scale=ch*1000/360;
        bitmap_fill(bitmaps[slot].words,drawing,(cw-600*scale/1000)/2+x*scale/1000,
            (ch-360*scale/1000)/2+y*scale/1000,w*scale/1000,h*scale/1000);
    }
}
static const struct vehicle_scene_ops scene_ops={image_load,image_unload,image_invalidate,image_draw};

static int create(struct dashborne_widget_view *v) {
    if(v->instance>=36)return -1;
    instances[v->instance]=v;++users;
    widgets->text(v->action,preview?"Stop":"Preview");return 0;
}
static void update(struct dashborne_widget_view *v,unsigned long long now) {
    if(!v->visible || failed)return;
    if(v->editing && preview){preview=0;next_sample=0;}
    widgets->text(v->action,preview?"Stop":"Preview");
    if(last_tick==now)return;last_tick=now;
    if(!scene.active && vehicle_scene_open(&scene,&scene_ops,0)) {
        failed=1;widgets->text(v->status,"Car images unavailable");return;
    }
    if(!next_sample || now>=next_sample || next_sample-now>100000) {
        next_sample=now+100000;struct readings r;const char *sample=0;
        if(preview)sample=sample_read(&r,now>=started?now-started:0);else vehicle_read(&r);
        vehicle_scene_update(&scene,r.connected && !r.demo,r.gear,r.body_known,r.body_open,now);
        char line[100];
        if(sample)snprintf(line,sizeof(line),"Preview: %s",sample);
        else if(!scene.connected)snprintf(line,sizeof(line),"Waiting for vehicle");
        else if(!scene.panels_known)snprintf(line,sizeof(line),"Panel status unavailable");
        else snprintf(line,sizeof(line),"Live: %s",r.gear==1?"Park":r.gear==2?"Reverse":r.gear==3?"Neutral":"Drive");
        for(int i=0;i<36;++i)if(instances[i])widgets->text(instances[i]->status,scene.body_failed?"Car image unavailable":line);
    }
    vehicle_scene_tick(&scene,now);
}
static void draw(struct dashborne_widget_view *v,void *surface) {
    if(scene.active){painting=v;drawing=surface;vehicle_scene_draw(&scene);drawing=0;painting=0;}
}
static void click(struct dashborne_widget_view *v) {
    (void)v;preview=!preview;started=source_microseconds();next_sample=0;
}
static void destroy(struct dashborne_widget_view *v) {
    if(v->instance<36 && instances[v->instance]) {instances[v->instance]=0;--users;}
    if(!users){vehicle_scene_close(&scene);preview=failed=0;last_tick=next_sample=0;}
}
static void suspend(struct dashborne_widget_view *v) {
    (void)v;vehicle_scene_close(&scene);preview=0;last_tick=next_sample=0;
}
static int activate(const struct dashborne_host *api) {
    host=api;widgets=dashborne_widget_api();if(!widgets || source_bind(api))return -1;
#define UI(v,s) do {*(void **)(&(v))=api->resolve(api->ui_runtime,s);if(!(v))return -1;}while(0)
    UI(bitmap_ctor,"_ZN13BitmapWrapperC1Ev");UI(bitmap_dtor,"_ZN13BitmapWrapperD1Ev");
    UI(bitmap_load,"_ZN13BitmapWrapper18loadBitmapFromFileEPKcii");
    UI(bitmap_fill,"_ZN13BitmapWrapper17fillBoxWithBitmapEPviiii");
#undef UI
    static const struct dashborne_widget w={1,sizeof(w),"vehicle-widget","3D Vehicle",
        "Animated vehicle state. Park, Drive, doors, frunk and trunk.",
        DBW_ALL_SIZES,DBW_SPECIAL,1,create,update,draw,click,destroy,suspend};
    return widgets->register_widget(api,&w);
}
static int deactivate(void) {
    if(widgets && widgets->unregister_widget(host))return -1;
    vehicle_scene_close(&scene);host=0;widgets=0;return 0;
}
DASHBORNE_EXTENSION_EXPORT const struct dashborne_extension *dashborne_extension_v1(void) {
    static const struct dashborne_extension extension={1,sizeof(extension),"vehicle-widget",activate,deactivate};return &extension;
}
