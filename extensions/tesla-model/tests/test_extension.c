#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../extension.c"
static int live,peak,loads,frees,repaints,hidden,read_count,action_alpha=255,canvas_alpha,opacity_calls;
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
static void opacity(void *p,unsigned char a){++opacity_calls;assert(a==0||a==255);if(p==&action_control)action_alpha=a;else canvas_alpha=a;}
static void text(void *p,const char *s){(void)p;assert(!*s);}
static void invalidate(struct dashborne_widget_view *v){assert(v);++repaints;}
static void log_message(const struct dashborne_host *h,const char *s){(void)h;(void)s;}
static int fills,lamp_boxes;
static void brush_color(void *p,unsigned int color){(void)p;assert((color>>24)==255 && (color&0xffffff));}
static void clipped_box(void *p,int x,int y,int w,int h){(void)p;assert(x>=0&&y>=0&&x+w<=486&&y+h<=434);assert(w==3&&h==3);++lamp_boxes;}
static void fill(void *b,void *p,int x,int y,int w,int h){(void)b;(void)p;(void)x;(void)y;assert(w>0&&h>0);++fills;}
int main(void) {
    struct dashborne_host api={.extension_directory="/tmp/test",.log=log_message};host=&api;
    struct dashborne_widgets sdk={.text=text,.invalidate=invalidate};widgets=&sdk;
    bitmap_ctor=ctor;bitmap_dtor=dtor;bitmap_load=load;position=pos;visible=vis;
    picture=pic;alpha=opacity;pack.count=5;
    brush=brush_color;fill_box=clipped_box;bitmap_fill=fill;
    const char *colors[]={"gray","white","black","blue","red"};
    for(int i=0;i<5;++i)snprintf(pack.ids[i],48,"highland-%s",colors[i]);
    struct dashborne_widget_view a={.instance=0,.width=510,.height=458,.visible=1,.action=&action_control},b=a;b.instance=1;
    assert(!create(&a)&&!create(&b)&&users==2&&hidden==6);
    assert(action_alpha==0&&canvas_alpha==255); /* The touch target must not cover the scene. */
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
    draw(&a,(void *)1);assert(fills>0&&!lamp_boxes);
    int previous_fills=fills;a.width=0;draw(&a,(void *)1);assert(fills==previous_fills);a.width=510;
    states[0].scene.angle=0;states[0].scene.data.lights_known=states[0].scene.data.low_beam=1;
    draw(&a,(void *)1);assert(lamp_boxes>0);
    /* Disconnect clears telemetry, never dims the model or the stock frame. */
    reading.connected=0;update(&a,3000000);assert(canvas_alpha==255);
    int previous_boxes=lamp_boxes;draw(&a,(void *)1);assert(lamp_boxes==previous_boxes);
    reading.connected=1;
    struct tm_layer missing={.name="missing",.frame=-1};
    assert(!get_image(0,&missing));int before=loads;assert(!get_image(0,&missing)&&loads==before);
    suspend(&a);assert(!live);suspend(&b);assert(!live);
    update(&a,500000000);assert(states[0].scene.angle==0);
    destroy(&a);assert(users==1&&!a.user);destroy(&a);assert(users==1);
    destroy(&b);assert(!users&&!live&&action_alpha==255);assert(frees==loads);
    assert(!create(&a));assert(states[0].variant==1);destroy(&a);
    /* Repeated page recycling restores shared controls and frees cached frames. */
    for(int page=0;page<100;++page) {
        assert(!create(&a));reading.gear=page%2?TM_DRIVE:TM_PARK;
        reading.connected=page%3!=0;update(&a,600000000ull+page*1000000ull);
        draw(&a,(void *)1);assert(canvas_alpha==255&&action_alpha==0);
        suspend(&a);destroy(&a);assert(!users&&!live&&action_alpha==255);
    }
    assert(peak<=CACHE_SIZE);
    puts("Tesla Model: bounded shared cache, model cycling, visibility, reuse and teardown passed");
}
