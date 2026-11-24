#include "dashborne_widget.h"
#include "scene.h"
#include "data.h"
#include "pack.h"
typedef __SIZE_TYPE__ usize;
extern int snprintf(char *,usize,const char *,...);
extern int strcmp(const char *,const char *);
extern void *memset(void *,int,usize);
#define CACHE_SIZE 12
struct rect {int x,y,w,h;};
struct bitmap {
    union {double align;unsigned char bytes[32];} object;
    char name[64];int variant,constructed,loaded;
    unsigned long long age;
};
struct state {struct tm_scene scene;struct dashborne_widget_view *view;int variant,opacity;unsigned long long next;};
static struct state states[36];
static struct bitmap cache[CACHE_SIZE];
static unsigned long long age;
static int users,selected_variant;
static struct tm_pack pack;
static const struct dashborne_host *host;
static const struct dashborne_widgets *widgets;
static void (*bitmap_ctor)(void *),(*bitmap_dtor)(void *);
static int (*bitmap_load)(void *,const char *,int,int);
static void (*bitmap_fill)(void *,void *,int,int,int,int);
static void (*position)(void *,const struct rect *),(*visible)(void *,int);
static void (*picture)(void *,int,const char *),(*alpha)(void *,unsigned char);
static void *(*lock_dc)(void *,const void *,int *,int *,int *);
static void (*unlock_dc)(void *);
static char preview[512];
static void cache_clear(void) {
    for(int i=0;i<CACHE_SIZE;++i) {
        if(cache[i].constructed)bitmap_dtor(&cache[i].object);
        cache[i]=(struct bitmap){0};
    }
    age=0;
}
static struct bitmap *get_image(int variant,const struct tm_layer *layer) {
    char name[64];
    if(layer->frame<0)snprintf(name,sizeof(name),"%s.png",layer->name);
    else snprintf(name,sizeof(name),"%s%d.png",layer->name,layer->frame);
    int slot=0;
    for(int i=0;i<CACHE_SIZE;++i) {
        struct bitmap *b=&cache[i];
        if(b->constructed && b->variant==variant && !strcmp(b->name,name)){b->age=++age;return b->loaded?b:0;}
        if(!b->constructed || b->age<cache[slot].age)slot=i;
    }
    struct bitmap *b=&cache[slot];
    if(b->constructed)bitmap_dtor(&b->object);
    *b=(struct bitmap){.variant=variant,.age=++age};
    snprintf(b->name,sizeof(b->name),"%s",name);
    char path[512];int n=snprintf(path,sizeof(path),"%s/models/%s/%s",host->extension_directory,pack.ids[variant],name);
    if(n<0 || (usize)n>=sizeof(path))return 0;
    bitmap_ctor(&b->object);b->constructed=1;
    b->loaded=tm_image_valid(path) && bitmap_load(&b->object,path,0,0);
    if(!b->loaded){host->log(host,"tesla-model: missing or invalid model frame");return 0;}
    return b;
}
static int create(struct dashborne_widget_view *v) {
    if(v->instance>=36)return -1;
    struct state *s=&states[v->instance];*s=(struct state){.view=v,.variant=selected_variant,.opacity=-1};v->user=s;++users;
    position(v->canvas,&(struct rect){12,12,v->width-24,v->height-24});
    visible(v->title,0);visible(v->value,0);visible(v->status,0);
    widgets->text(v->action,"");alpha(v->action,0);
    for(int i=0;i<5;++i)picture(v->action,i,"");
    /* Native click handling still owns gesture cancellation; the runtime hides
       the hit area in edit mode so selection, dragging and removal stay native. */
    position(v->action,&(struct rect){12,12,v->width-24,v->height-24});
    return 0;
}
static void update(struct dashborne_widget_view *v,unsigned long long now) {
    struct state *s=v->user;if(!s || !v->visible)return;
    if(s->next && now<s->next && s->next-now<100000)return;
    s->next=now+25000;struct tm_reading r;tm_data_read(&r);
    int opacity=r.connected?255:150;
    if(s->opacity!=opacity){alpha(v->canvas,(unsigned char)opacity);s->opacity=opacity;}
    if(tm_step(&s->scene,&r,now))widgets->invalidate(v);
}
static void draw(struct dashborne_widget_view *v,void *surface) {
    struct state *s=v->user;if(!s)return;
    int cw=v->width-24,ch=v->height-24,w=0,h=0,pitch=0;
    struct rect bounds={0,0,cw,ch};
    unsigned int *pixels=lock_dc(surface,&bounds,&w,&h,&pitch);
    if(!pixels)return;
    if(w<=0 || h<=0 || w>cw || h>ch || pitch<w*4){unlock_dc(surface);return;}
    for(int y=0;y<h;++y)for(int x=0;x<w;++x)pixels[y*(pitch/4)+x]=0xff000000u;
    unlock_dc(surface);
    struct tm_layer layers[TM_LAYERS];int count=tm_layers(&s->scene,layers);
    int scale=cw*1000/510;if(ch*1000/458<scale)scale=ch*1000/458;
    int ox=(cw-510*scale/1000)/2,oy=(ch-458*scale/1000)/2;
    for(int i=0;i<count;++i) {
        const struct tm_layer *l=&layers[i];struct bitmap *b=get_image(s->variant,l);if(!b)continue;
        bitmap_fill(&b->object,surface,ox+l->x*scale/1000,oy+l->y*scale/1000,l->w*scale/1000,l->h*scale/1000);
    }
    struct tm_lamp lamps[4];int lamp_count=tm_lamps(&s->scene,lamps);
    if(!lamp_count)return;
    pixels=lock_dc(surface,&bounds,&w,&h,&pitch);if(!pixels)return;
    if(w<=0 || h<=0 || w>cw || h>ch || pitch<w*4){unlock_dc(surface);return;}
    for(int i=0;i<lamp_count;++i) {
        const struct tm_lamp *l=&lamps[i];
        int x0=ox+l->x0*scale/1000,y0=oy+l->y0*scale/1000;
        int dx=(l->x1-l->x0)*scale/1000,dy=(l->y1-l->y0)*scale/1000;
        int steps=dx<0?-dx:dx,ady=dy<0?-dy:dy;if(ady>steps)steps=ady;
        if(!steps)continue;
        for(int k=0;k<=steps;++k)for(int yy=-1;yy<=1;++yy)for(int xx=-1;xx<=1;++xx) {
            int x=x0+dx*k/steps+xx,y=y0+dy*k/steps+yy;
            if(x>=0&&y>=0&&x<w&&y<h)pixels[y*(pitch/4)+x]=0xff000000u|l->color;
        }
    }
    unlock_dc(surface);
}
static void click(struct dashborne_widget_view *v) {
    struct state *s=v->user;if(!s || v->editing)return;
    selected_variant=(selected_variant+1)%pack.count;
    for(int i=0;i<36;++i)if(states[i].view) {
        states[i].variant=selected_variant;widgets->invalidate(states[i].view);
    }
}
static void destroy(struct dashborne_widget_view *v) {
    struct state *s=v->user;if(!s)return;
    *s=(struct state){0};v->user=0;
    /* Restore controls that another provider can receive when this pool slot
       is reused. The core will restore sizes and labels on its next assignment. */
    visible(v->title,1);visible(v->value,1);visible(v->status,1);alpha(v->canvas,255);
    alpha(v->action,255);
    if(users>0)--users;if(!users)cache_clear();
}
static void suspend(struct dashborne_widget_view *v) {
    struct state *s=v->user;if(s){s->next=0;s->scene.tick=s->scene.road_clock=0;}
    cache_clear();
}
static int activate(const struct dashborne_host *api) {
    host=api;widgets=dashborne_widget_api();if(!widgets || tm_data_bind(api))return -1;
    if(tm_pack_load(&pack,api->extension_directory)) {
        api->log(api,"tesla-model: install a model pack in this extension's models directory");return -1;
    }
#define UI(v,s) do {*(void **)(&(v))=api->resolve(api->ui_runtime,s);if(!(v)){api->log(api,"tesla-model: missing " s);return -1;}}while(0)
    UI(bitmap_ctor,"_ZN13BitmapWrapperC1Ev");UI(bitmap_dtor,"_ZN13BitmapWrapperD1Ev");
    UI(bitmap_load,"_ZN13BitmapWrapper18loadBitmapFromFileEPKcii");UI(bitmap_fill,"_ZN13BitmapWrapper17fillBoxWithBitmapEPviiii");
    UI(position,"_ZN6ZKBase11setPositionERK14LayoutPosition");UI(visible,"_ZN6ZKBase10setVisibleEb");
    UI(alpha,"_ZN6ZKBase8setAlphaEh");
    UI(picture,"_ZN8ZKButton18setButtonStatusPicEiPKc");
    UI(lock_dc,"zk_gdi_lock_dc");UI(unlock_dc,"zk_gdi_unlock_dc");
#undef UI
    int n=snprintf(preview,sizeof(preview),"../../..%s/models/%s/P_N0.png",api->extension_directory,pack.ids[0]);
    if(n<0 || (usize)n>=sizeof(preview))return -1;
    static struct dashborne_widget widget={
        .abi_version=DASHBORNE_WIDGET_ABI,.struct_size=sizeof(widget),
        .id="tesla-model",.title="Tesla Model Widget",.description="Animated vehicle state. Tap to change model and color.",
        .sizes=1u<<DBW_2X2,.tab=DBW_SPECIAL,.category=1,
        .create=create,.update=update,.draw=draw,.click=click,.destroy=destroy,.suspend=suspend
    };
    widget.preview_images[DBW_2X2]=preview;
    return widgets->register_widget(api,&widget);
}
static int deactivate(void) {
    if(widgets && widgets->unregister_widget(host))return -1;
    cache_clear();users=selected_variant=0;host=0;widgets=0;return 0;
}
DASHBORNE_EXTENSION_EXPORT const struct dashborne_extension *dashborne_extension_v1(void) {
    static const struct dashborne_extension extension={DASHBORNE_EXTENSION_ABI,sizeof(extension),"tesla-model",activate,deactivate};return &extension;
}
