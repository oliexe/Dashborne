#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../extension.c"
static int live,peak,loads,frees,repaints,hidden,read_count,action_alpha=255,canvas_alpha,opacity_calls;
static unsigned char action_control;
static unsigned int action_background=2237484;
static struct tm_reading reading={.connected=1,.gear=TM_DRIVE,.known=63};
int tm_data_bind(const struct dashborne_host *h){(void)h;return 0;}
void tm_data_read(struct tm_reading *r){*r=reading;++read_count;}
int tm_image_valid(const char *p){(void)p;return 1;}
int tm_pack_load(struct tm_pack *p,const char *dir){(void)p;(void)dir;return 0;}
static void image_size(const void *p,int *w,int *h){(void)p;*w=149;*h=361;}
static void ctor(void *p){(void)p;++live;if(live>peak)peak=live;}
static void dtor(void *p){(void)p;assert(live>0);--live;++frees;}
static int load(void *p,const char *name,int w,int h){(void)p;(void)w;(void)h;assert(strstr(name,"/models/highland-")||strstr(name,"/assets/"));++loads;return !strstr(name,"missing");}
static void pos(void *p,const struct rect *r){(void)p;assert(r->w>0&&r->h>0);}
static void vis(void *p,int v){(void)p;if(!v)++hidden;}
static void pic(void *p,int n,const char *s){(void)p;(void)n;assert(!*s);}
static void opacity(void *p,unsigned char a){++opacity_calls;assert(a==0||a==255);if(p==&action_control)action_alpha=a;else canvas_alpha=a;}
static void bg(void *p,unsigned int color){assert(p==&action_control);action_background=color;}
static void text(void *p,const char *s){(void)p;assert(!*s);}
static void invalidate(struct dashborne_widget_view *v){assert(v);++repaints;}
static void log_message(const struct dashborne_host *h,const char *s){(void)h;(void)s;}
static int fills,lamp_boxes,backing_rows;
static unsigned int theme_words[140],last_brush,dc_words[64],road_buffer[486*434];
static const void *theme_config(void){return theme_words;}
static void brush_color(void *p,unsigned int color){(void)p;last_brush=color;dc_words[4]=color;assert((color>>24)==255 && (color&0xffffff));}
static void clipped_box(void *p,int x,int y,int w,int h){
    (void)p;assert(x>=0&&y>=0&&x+w<=486&&y+h<=434);
    if(w==3&&h==3)++lamp_boxes;
    else if(w==88&&x==199&&y>=173&&y+h<=261)backing_rows+=h;
}
static void fill(void *b,void *p,int x,int y,int w,int h){(void)b;(void)p;(void)x;(void)y;assert(w>0&&h>0);++fills;}
static void *mock_dc(void *surface){(void)surface;((unsigned char *)dc_words)[0x30]=255;return dc_words;}
static int mock_clips(void *surface,const struct clip_region *region,void *context,clip_draw draw){
 assert(region->head && !region->head->next);
 struct clip_rect clip={0,0,486,434};draw(surface,(unsigned char *)road_buffer,486*4,4,&clip,context);return 1;
}
static void mock_unlock(void *surface){(void)surface;}
static void test_road_clipping(void) {
 struct tm_scene scene={.data={.detected_left=1,.detected_right=1}};
 struct road_paint p={.scene=&scene,.origin_x=-100,.origin_y=-20,.scale=1000,.alpha=255,.colors={0xffe5a044,0xffe5a044}};
 struct clip_rect clip={0,50,200,420};
 for(unsigned int i=0;i<486*434;++i)road_buffer[i]=0xff112233;
 road_pixels(0,(unsigned char *)(road_buffer+50*486),486*4,4,&clip,&p);
 int painted=0;
 for(int y=0;y<434;++y)for(int x=0;x<486;++x) {
  unsigned int value=road_buffer[y*486+x];
  if(x>=200 || y<50 || y>=420)assert(value==0xff112233);
  else painted+=value!=0xff112233;
 }
 assert(painted>0);
 unsigned int saved[486*434];memcpy(saved,road_buffer,sizeof(saved));
 road_pixels(0,(unsigned char *)road_buffer,486*4,2,&clip,&p);
 assert(!memcmp(saved,road_buffer,sizeof(saved)));
 p.origin_y=500;p.origin_x=-600;
 road_pixels(0,(unsigned char *)(road_buffer+50*486),486*4,4,&clip,&p);
 assert(!memcmp(saved,road_buffer,sizeof(saved)));
}
int main(void) {
    checked_dc=mock_dc;lock_clips=mock_clips;unlock_dc=mock_unlock;test_road_clipping();
    struct dashborne_host api={.extension_directory="/tmp/test",.log=log_message};host=&api;
    struct dashborne_widgets sdk={.text=text,.invalidate=invalidate};widgets=&sdk;
    bitmap_size=image_size;bitmap_ctor=ctor;bitmap_dtor=dtor;bitmap_load=load;position=pos;visible=vis;
    picture=pic;alpha=opacity;background=bg;pack.count=5;
    brush=brush_color;fill_box=clipped_box;bitmap_fill=fill;
    const char *colors[]={"gray","white","black","blue","red"};
    for(int i=0;i<5;++i)snprintf(pack.ids[i],48,"highland-%s",colors[i]);
    struct dashborne_widget_view a={.instance=0,.width=510,.height=458,.visible=1,.action=&action_control},b=a;b.instance=1;
    assert(!create(&a)&&!create(&b)&&users==2&&hidden==6);
    assert(action_alpha==0&&canvas_alpha==255); /* The touch target must not cover the scene. */
    assert(action_background==0); /* Parent dimming may override child alpha. */
    click(&a);assert(states[0].variant==1&&states[1].variant==1);
    a.editing=1;click(&a);assert(states[0].variant==1);a.editing=0;
    for(int n=0;n<50;++n)click(&a);assert(states[0].variant==1);
    update(&a,1000000);int reads=read_count;update(&a,1000001);assert(read_count==reads);
    int previous_opacity_calls=opacity_calls;update(&a,1025000);assert(opacity_calls==previous_opacity_calls);reads=read_count;
    /* Each 16 ms native timer callback advances the animation. A late callback does not replay missed ticks. */
    states[0].next=0;update(&a,4000000);int cadence_reads=read_count;
    for(unsigned long long t=4016000;t<=6000000;t+=16000)update(&a,t);
    assert(read_count-cadence_reads==125);
    cadence_reads=read_count;update(&a,9000000);assert(read_count==cadence_reads+1);
    states[0].next=0;reads=read_count;
    a.visible=0;update(&a,2000000);assert(read_count==reads);a.visible=1;
    for(int variant=0;variant<5;++variant)for(int frame=0;frame<25;++frame) {
        struct tm_layer l={.name="P_N",.frame=frame};
        assert(get_image(variant,&l));int before=loads;assert(get_image(variant,&l));assert(loads==before);
    }
    assert(peak==MODEL_CACHE_SIZE&&live==MODEL_CACHE_SIZE);
    for(int frame=0;frame<20;++frame){struct tm_layer road={.name="road",.frame=frame};assert(get_image(0,&road));}
    int road_loads=loads;
    for(int lap=0;lap<3;++lap)for(int frame=0;frame<20;++frame) {
        struct tm_layer road={.name="road",.frame=frame};assert(get_image(0,&road));
    }
    assert(loads==road_loads&&peak==CACHE_SIZE&&live==CACHE_SIZE);
    draw(&a,(void *)1);assert(fills>0&&!lamp_boxes&&backing_rows==88);
    active_theme=theme_config;theme_words[0x20c/4]=0x101010;
    draw(&a,(void *)1);assert(backing_rows==176);
    theme_words[0x20c/4]=0xffffff;
    draw(&a,(void *)1);assert(backing_rows==264);
    int previous_fills=fills;a.width=0;draw(&a,(void *)1);assert(fills==previous_fills);a.width=510;
    states[0].scene.angle=0;states[0].scene.approach.value=0;states[0].scene.data.lights_known=states[0].scene.data.low_beam=1;
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
    destroy(&b);assert(!users&&!live&&action_alpha==255&&action_background==2237484);assert(frees==loads);
    assert(!create(&a));assert(states[0].variant==1);destroy(&a);
    /* Repeated page recycling restores shared controls and frees cached frames. */
    for(int page=0;page<100;++page) {
        assert(!create(&a));reading.gear=page%2?TM_DRIVE:TM_PARK;
        reading.connected=page%3!=0;update(&a,600000000ull+page*1000000ull);
        draw(&a,(void *)1);assert(canvas_alpha==255&&action_alpha==0&&action_background==0);
        suspend(&a);destroy(&a);assert(!users&&!live&&action_alpha==255&&action_background==2237484);
    }
    assert(peak<=CACHE_SIZE);
    puts("Tesla Model: bounded shared cache, model cycling, visibility, reuse and teardown passed");
}
