#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../extension.c"
static int live_bitmaps,bitmap_peak,painted,invalidations;
static unsigned long long now=1000000;
static struct readings input={.connected=1,.gear=1,.body_known=63};
static char status_value[100];
int source_bind(const struct dashborne_host *h){(void)h;return 0;}
unsigned long long source_microseconds(void){return now;}
void vehicle_read(struct readings *r){*r=input;}
static void label(void *c,const char *s){if(c==(void *)2)snprintf(status_value,sizeof(status_value),"%s",s);}
static void dirty(struct dashborne_widget_view *v){assert(v);++invalidations;}
static void init_bitmap(void *p){((void **)p)[0]=(void *)1;++live_bitmaps;if(live_bitmaps>bitmap_peak)bitmap_peak=live_bitmaps;}
static void free_bitmap(void *p){assert(((void **)p)[0]==(void *)1);((void **)p)[0]=0;--live_bitmaps;}
static int read_bitmap(void *p,const char *path,int a,int b){assert(p && strstr(path,"/assets/motion/") && !a && !b);return 1;}
static void paint_bitmap(void *p,void *surface,int x,int y,int w,int h){assert(p && surface==(void *)3 && x>=0 && y>=0 && x+w<=painting->width-24 && y+h<=painting->height-108);++painted;}
static void tick(struct dashborne_widget_view *v,unsigned long long delta){now+=delta;update(v,now);draw(v,(void *)3);}
int main(void) {
    const struct dashborne_host h={.extension_directory="/test/vehicle-widget"};host=&h;
    const struct dashborne_widgets sdk={.text=label,.invalidate=dirty};widgets=&sdk;
    bitmap_ctor=init_bitmap;bitmap_dtor=free_bitmap;bitmap_load=read_bitmap;bitmap_fill=paint_bitmap;
    struct dashborne_widget_view a={.instance=0,.width=510,.height=458,.visible=1,.status=(void *)2};
    struct dashborne_widget_view b=a;b.instance=1;
    assert(!create(&a));tick(&a,1);assert(live_bitmaps==65 && painted && !strcmp(status_value,"Live: Park"));
    assert(!create(&b));tick(&b,100000);assert(live_bitmaps==65 && users==2);
    int writes=invalidations;for(int i=0;i<100;++i)tick(&a,16667);assert(invalidations==writes);
    for(int i=0;i<30;++i) {
        click(&a);assert(preview);tick(&a,2500000);click(&a);assert(!preview);
        tick(&a,1);tick(&a,1200000);assert(scene.frame==0 && !failed);
    }
    input.body_open=63;tick(&a,100000);tick(&a,500000);assert(scene.body==63 && bitmap_peak<=77);
    input.connected=0;tick(&a,100000);assert(!scene.body && scene.frame==0 && !strcmp(status_value,"Waiting for vehicle"));
    a.editing=1;click(&a);tick(&a,100000);assert(!preview);
    const int sizes[][2]={{250,224},{510,224},{250,458},{510,458},{770,458},{1030,458},{1290,458},{1550,458}};
    for(int i=0;i<8;++i){a.width=sizes[i][0];a.height=sizes[i][1];tick(&a,100000);}
    suspend(&a);assert(!live_bitmaps);tick(&a,100000);assert(live_bitmaps);
    destroy(&a);assert(live_bitmaps && users==1);destroy(&b);assert(!users && !live_bitmaps);
    puts("Vehicle provider: all sizes, shared cache, poses, Preview/Stop, unavailable data and release passed");
}
