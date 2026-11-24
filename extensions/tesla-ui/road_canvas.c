#include "road_canvas.h"
#include "road.h"
/* Only our road control gets a private vtable. source_bind checks the runtime
   fingerprint before this adapter is used; native destruction keeps its ABI. */
static void **original,*table[13],*control;
struct layout_position {int left,top,width,height;};
static void (*draw_original)(void *,void *);
static void (*invalidate)(void *,const struct layout_position *);
static void (*brush)(void *,unsigned int),(*box)(void *,int,int,int,int);
static float phase;
static int queued;
static unsigned long long last_request;
static void draw(void *self,void *canvas) {
    draw_original(self,canvas);
    brush(canvas,0xff73777e);
    for(int i=0;i<ROAD_MARKS;++i) {
        struct road_mark m=road_mark(phase,i);
        /* Rasterize the perspective stripe in the fixed road canvas. No
           control movement, image rescaling, allocation or layout messages. */
        for(int y=0;y<m.height;++y) {
            int dx=(m.width-3)*y/(m.height?m.height:1);
            int x=m.x-680+(i<6?m.width-3-dx:dx);
            box(canvas,x,m.y-55+y,2,1);
        }
    }
    queued=0;
}
int road_canvas_bind(const struct dashborne_host *h) {
#define BIND(v,s) do {*(void **)(&(v))=h->resolve(h->ui_runtime,s);if(!(v)){h->log(h,"tesla-ui: road missing symbol " s);return -1;}}while(0)
    BIND(original,"_ZTV10ZKTextView");
    BIND(draw_original,"_ZN10ZKTextView6onDrawEP8ZKCanvas");
    BIND(invalidate,"_ZN6ZKBase10invalidateEPK14LayoutPosition");
    BIND(brush,"SetBrushColor");BIND(box,"zk_gdi_fill_box");
#undef BIND
    if(original[10]!=(void *)draw_original){h->log(h,"tesla-ui: road draw vtable mismatch");return -1;}
    for(int i=0;i<13;++i)table[i]=original[i];
    table[10]=(void *)draw;return 0;
}
int road_canvas_attach(void *p) {
    if(control || !p || *(void ***)p!=original+2)return -1;
    control=p;*(void ***)p=table+2;phase=0;queued=0;last_request=0;return 0;
}
void road_canvas_detach(void) {
    if(control && *(void ***)control==table+2)*(void ***)control=original+2;
    control=0;queued=0;last_request=0;
}
void road_canvas_frame(float next,unsigned long long now) {
    if(!control)return;
    int changed=phase!=next;phase=next;
    /* At most one outstanding repaint. A busy renderer consumes the newest
       phase instead of accumulating frames. The native timer can run at 10ms. */
    if(queued || !changed || (last_request && now>=last_request && now-last_request<16667))return;
    queued=1;last_request=now;invalidate(control,0);
}
