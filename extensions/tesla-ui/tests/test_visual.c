#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../visual.c"
static void *native[13];static void **car,**label;
static int loaded,destroyed,requests,draws,parts,blits,fail_at=-1,attempts;
static void original_draw(void *p,void *dc) {assert((p==&car || p==&label) && dc==(void *)7);++draws;}
static void invalid(void *p,const struct rectangle *r) {assert((p==&car || p==&label) && !r);++requests;}
static void ctor(void *p) {((void **)p)[0]=(void *)123;}
static void dtor(void *p) {assert(((void **)p)[0]==(void *)123);++destroyed;}
static int bitmap_read(void *p,const char *path,int a,int b) {
    assert(((void **)p)[0]==(void *)123 && strstr(path,"/assets/") && !a && !b);++attempts;
    if(attempts==fail_at)return 0;
    ++loaded;return 1;
}
static void text_clear(void *p,const char *s) {assert(p==&label && !*s);}
static void pic_clear(void *p,const char *s) {assert(p==&car && !*s);}
static void fill(void *b,void *dc,int x,int y,int w,int h) {
    assert(b && dc==(void *)7 && x>=0 && y>=0 && w>0 && h>0 && x+w<=600 && y+h<=360);++blits;
}
static void part(void *b,void *dc,int x,int y,int w,int h,int sx,int sy) {
    (void)x;(void)y;assert(b && dc==(void *)7 && w>0 && h>0 && sx>=0 && sy>=0);++parts;
}
static void *resolve(void *handle,const char *s) {
    (void)handle;
#define R(name,fn) if(!strcmp(s,name))return (void *)fn
    R("_ZTV10ZKTextView",native);R("_ZN10ZKTextView6onDrawEP8ZKCanvas",original_draw);
    R("_ZN6ZKBase10invalidateEPK14LayoutPosition",invalid);
    R("_ZN13BitmapWrapperC1Ev",ctor);R("_ZN13BitmapWrapperD1Ev",dtor);
    R("_ZN13BitmapWrapper18loadBitmapFromFileEPKcii",bitmap_read);
    R("_ZN13BitmapWrapper17fillBoxWithBitmapEPviiii",fill);
    R("_ZN13BitmapWrapper21fillBoxWithBitmapPartEPviiiiii",part);
    R("_ZN10ZKTextView7setTextEPKc",text_clear);R("_ZN6ZKBase16setBackgroundPicEPKc",pic_clear);
#undef R
    assert(0);return 0;
}
static void log_line(const struct dashborne_host *h,const char *s) {(void)h;assert(strstr(s,"tesla-ui"));}
static void paint(void) {if(controls[TYPE_LABELS].queued)draw(&car,(void *)7);}
int main(void) {
    struct dashborne_host h={.resolve=resolve,.log=log_line};
    assert(visual_bind(&h)==-1);native[10]=(void *)original_draw;assert(!visual_bind(&h));
    car=label=native+2;
    assert(visual_attach(599318,&label));assert(visual_text(&label,"312 km · 21°C"));
    draw(&label,(void *)7);assert(parts==13);
    int before=requests;assert(visual_text(&label,"312 km · 21°C") && requests==before);
    assert(visual_attach(599306,&car));assert(loaded==220);
    assert(visual_vehicle(0,0,1000000,1));paint();assert(current==0 && blits==1);
    int before_load=loaded;
    for(int i=1;i<100;++i)visual_vehicle(0,0,1000000+i*100000ull,1);
    assert(loaded==before_load); /* Waiting must not repeatedly decode images. */
    visual_vehicle(1,0,12000000,0);assert(loaded==before_load);
    before=requests;int before_draw=draws;
    for(int i=0;i<150;++i) {
        visual_frame(12000000+i*10000ull);
        assert(requests-before<=draws-before_draw+1);
        if(i%10==0)paint(); /* Deliberately slow renderer: no queue growth. */
    }
    paint();assert(current==64 && loaded==before_load);
    visual_vehicle(0,0,14000000,0);visual_frame(14500000);int f=current;
    visual_vehicle(1,0,14500000,0);assert(current==f);
    visual_frame(16000000);paint();assert(current==64);
    visual_vehicle(0,0,17000000,1);paint();
    visual_vehicle(0,4,18000000,0);
    visual_frame(18225000);paint();assert(hinge_position>.4f && hinge_position<.6f);
    visual_frame(18500000);paint();assert(hinge_position==1);
    visual_vehicle(0,0,19000000,0);visual_frame(19500000);paint();assert(body_mask==0 && hinge_position==0);
    visual_vehicle(1,0,20000000,0);assert(current==0); /* Closed doors must not disable camera animation. */
    visual_frame(22000000);paint();assert(current==64);
    visual_detach();assert(car==native+2 && label==native+2 && loaded==destroyed);
    before=requests;visual_frame(23000000);assert(requests==before);
    fail_at=attempts+17;assert(!visual_attach(599306,&car));assert(car==native+2 && !ready);
    assert(destroyed==loaded+1); /* Failed load's wrapper is released, too. */
    visual_detach();return 0;
}
