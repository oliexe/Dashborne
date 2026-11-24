#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../extension.c"
static int live,peak,loads,frees,repaints,hidden,read_count,action_alpha=255,opacity_calls;
static unsigned char action_control;
static struct tm_reading reading={.connected=1,.gear=TM_DRIVE,.known=63};
int tm_data_bind(const struct dashborne_host *h){(void)h;return 0;}
void tm_data_read(struct tm_reading *r){*r=reading;++read_count;}
int tm_image_valid(const char *p){(void)p;return 1;}
int tm_pack_load(struct tm_pack *p,const char *dir){(void)p;(void)dir;return 0;}
static void ctor(void *p){(void)p;++live;if(live>peak)peak=live;}
static void dtor(void *p){(void)p;assert(live>0);--live;++frees;}
static int load(void *p,const char *name,int w,int h){(void)p;(void)w;(void)h;assert(strstr(name,"/models/highland-"));++loads;return !strstr(name,"missing");}
static void pos(void *p,const struct rect *r){(void)p;assert(r->w>0&&r->h>0);}
static void vis(void *p,int v){(void)p;if(!v)++hidden;}
static void pic(void *p,int n,const char *s){(void)p;(void)n;assert(!*s);}
static void opacity(void *p,unsigned char a){++opacity_calls;assert(a==0||a==150||a==255);if(p==&action_control)action_alpha=a;}
static void text(void *p,const char *s){(void)p;assert(!*s);}
static void invalidate(struct dashborne_widget_view *v){assert(v);++repaints;}
static void log_message(const struct dashborne_host *h,const char *s){(void)h;(void)s;}
static unsigned int framebuffer[486*434];static int locks,unlocks,fills,bad_surface;
static void *lock(void *p,const void *rect,int *w,int *h,int *pitch) {
    (void)p;(void)rect;++locks;*w=bad_surface?1920:486;*h=434;*pitch=486*4;return framebuffer;
}
static void unlock(void *p){(void)p;++unlocks;}
static void fill(void *b,void *p,int x,int y,int w,int h){(void)b;(void)p;(void)x;(void)y;assert(w>0&&h>0);++fills;}
int main(void) {
    struct dashborne_host api={.extension_directory="/tmp/test",.log=log_message};host=&api;
    struct dashborne_widgets sdk={.text=text,.invalidate=invalidate};widgets=&sdk;
    bitmap_ctor=ctor;bitmap_dtor=dtor;bitmap_load=load;position=pos;visible=vis;
    picture=pic;alpha=opacity;pack.count=5;
    lock_dc=lock;unlock_dc=unlock;bitmap_fill=fill;
    const char *colors[]={"gray","white","black","blue","red"};
    for(int i=0;i<5;++i)snprintf(pack.ids[i],48,"highland-%s",colors[i]);
    struct dashborne_widget_view a={.instance=0,.width=510,.height=458,.visible=1,.action=&action_control},b=a;b.instance=1;
    assert(!create(&a)&&!create(&b)&&users==2&&hidden==6);
    assert(action_alpha==0); /* The touch target must not cover the scene. */
    click(&a);assert(states[0].variant==1&&states[1].variant==1);
    a.editing=1;click(&a);assert(states[0].variant==1);a.editing=0;
    for(int n=0;n<50;++n)click(&a);assert(states[0].variant==1);
    update(&a,1000000);int reads=read_count;update(&a,1000001);assert(read_count==reads);
    int previous_opacity_calls=opacity_calls;update(&a,1025000);assert(opacity_calls==previous_opacity_calls);reads=read_count;
    a.visible=0;update(&a,2000000);assert(read_count==reads);a.visible=1;
    for(int variant=0;variant<5;++variant)for(int frame=0;frame<25;++frame) {
        struct tm_layer l={.name="P_N",.frame=frame};
        assert(get_image(variant,&l));int before=loads;assert(get_image(variant,&l));assert(loads==before);
    }
    assert(peak==CACHE_SIZE&&live==CACHE_SIZE);
    draw(&a,(void *)1);assert(locks==unlocks&&fills>0&&framebuffer[0]==0xff000000u);
    bad_surface=1;int previous_fills=fills;draw(&a,(void *)1);assert(locks==unlocks&&fills==previous_fills);bad_surface=0;
    states[0].scene.angle=0;states[0].scene.data.lights_known=states[0].scene.data.low_beam=1;
    int before_locks=locks;draw(&a,(void *)1);assert(locks==before_locks+2&&locks==unlocks);
    struct tm_layer missing={.name="missing",.frame=-1};
    assert(!get_image(0,&missing));int before=loads;assert(!get_image(0,&missing)&&loads==before);
    suspend(&a);assert(!live);suspend(&b);assert(!live);
    update(&a,500000000);assert(states[0].scene.angle==0);
    destroy(&a);assert(users==1&&!a.user);destroy(&a);assert(users==1);
    destroy(&b);assert(!users&&!live&&action_alpha==255);assert(frees==loads);
    assert(!create(&a));assert(states[0].variant==1);destroy(&a);
    puts("Tesla Model: bounded shared cache, model cycling, visibility, reuse and teardown passed");
}
