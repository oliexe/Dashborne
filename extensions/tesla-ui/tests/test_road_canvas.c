#include <assert.h>
#include <string.h>
#include "../road_canvas.c"
static void *native[13];static void **object;
static int invalidations,draws,boxes;
static void original_draw(void *self,void *dc) {assert(self==&object && dc==(void *)7);++draws;}
static void invalid(void *p,const struct layout_position *area) {assert(p==&object && !area);++invalidations;}
static void set_brush(void *dc,unsigned int c) {assert(dc==(void *)7 && c==0xff73777e);}
static void fill(void *dc,int x,int y,int w,int h) {assert(dc==(void *)7 && x>=0 && x+w<=600 && y>=0 && y+h<=360 && w==2 && h==1);++boxes;}
static void *resolve(void *handle,const char *symbol) {
    (void)handle;
    if(!strcmp(symbol,"_ZTV10ZKTextView"))return native;
    if(!strcmp(symbol,"_ZN10ZKTextView6onDrawEP8ZKCanvas"))return (void *)original_draw;
    if(!strcmp(symbol,"_ZN6ZKBase10invalidateEPK14LayoutPosition"))return (void *)invalid;
    if(!strcmp(symbol,"SetBrushColor"))return (void *)set_brush;
    if(!strcmp(symbol,"zk_gdi_fill_box"))return (void *)fill;
    assert(0);return 0;
}
static void log_message(const struct dashborne_host *h,const char *message) {(void)h;assert(strstr(message,"tesla-ui: road"));}
int main(void) {
    struct dashborne_host h={.resolve=resolve,.log=log_message};
    assert(road_canvas_bind(&h)==-1);native[10]=(void *)original_draw;
    assert(!road_canvas_bind(&h));object=native+2;
    assert(!road_canvas_attach(&object));assert(road_canvas_attach(&object)==-1);
    for(int tick=1;tick<=10000;++tick) {
        road_canvas_frame((float)(tick%120)/10,tick*10000ull);
        /* A very slow compositor: updates must coalesce, never queue up. */
        assert(invalidations<=draws+1);
        if(tick%10==0 && queued)((void (*)(void *,void *))object[8])(&object,(void *)7);
    }
    assert(draws==1000 && invalidations==1000 && boxes>1000);
    road_canvas_frame(3,100010000);assert(queued);
    road_canvas_detach();assert(object==native+2 && !queued);
    int before=invalidations;road_canvas_frame(5,100020000);assert(invalidations==before);
    assert(!road_canvas_attach(&object));road_canvas_detach();
    return 0;
}
