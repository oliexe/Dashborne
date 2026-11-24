#include "visual.h"
#include "motion.h"
#include "typography_data.h"
#include "motion_data.h"
extern int snprintf(char *,__SIZE_TYPE__,const char *,...);
extern int strcmp(const char *,const char *);
#define ASSETS "/mnt/extsd/Dashborne/extensions/tesla-ui/assets/"
/* BitmapWrapper is six ARM words in the fingerprinted runtime. Extra alignment
   and space also let the adapter run under the host ABI test doubles. */
struct bitmap {void *words[6];int initialized,loaded;};
struct paint_control {void *p;int id,type,queued;char value[100];};
struct rectangle {int x,y,w,h;};
static struct paint_control controls[TYPE_LABELS+1];
static struct bitmap fonts[TYPE_STYLES],camera[65],body,hinges[154];
static void **original,*table[13];
static void (*base_draw)(void *,void *),(*invalidate)(void *,const struct rectangle *);
static void (*bitmap_ctor)(void *),(*bitmap_dtor)(void *),(*clear_text)(void *,const char *);
static int (*bitmap_load)(void *,const char *,int,int);
static void (*bitmap_fill)(void *,void *,int,int,int,int);
static void (*bitmap_part)(void *,void *,int,int,int,int,int,int);
static void (*set_picture)(void *,const char *);
static struct camera_motion motion;
static int current=-1,ready,car_slot=-1,body_index=-1,body_mask,body_view,hinge_mask,hinge_view,hinge_target,dirty;
static float hinge_position,hinge_from;
static unsigned long long hinge_start,last_frame;
static int requested_mask;
static const struct dashborne_host *host;
static void release(struct bitmap *b) {
    if(b->initialized)bitmap_dtor(b->words);
    *b=(struct bitmap){0};
}
static int load(struct bitmap *b,const char *path) {
    release(b);bitmap_ctor(b->words);b->initialized=1;
    b->loaded=bitmap_load(b->words,path,0,0);return b->loaded;
}
static int sprite_load(struct bitmap *b,int index) {
    char path[192];snprintf(path,sizeof(path),ASSETS "motion/%s",vehicle_sprites[index].name);
    return load(b,path);
}
static void request(struct paint_control *c) {
    if(c->p && !c->queued){c->queued=1;invalidate(c->p,0);}
}
static const struct glyph *glyph(int style,unsigned int code) {
    for(int i=0;i<TYPE_GLYPHS;++i)if(glyph_styles[style][i].code==(int)code)return &glyph_styles[style][i];
    return &glyph_styles[style]['?'-32];
}
static unsigned int next(const unsigned char **s) {
    unsigned int c=*(*s)++;
    if(c<128)return c;
    if((c&224)==192 && (**s&192)==128){c=((c&31)<<6)|(*(*s)++&63);return c;}
    if((c&240)==224 && (**s&192)==128 && ((*s)[1]&192)==128){c=((c&15)<<12)|(((*s)[0]&63)<<6)|((*s)[1]&63);*s+=2;return c;}
    return '?';
}
static void draw_text(struct paint_control *c,void *canvas) {
    const struct type_label *t=&type_labels[c->type];
    if(c->id==599320) {
        const char *letters="PRND";int x=0;
        for(int i=0;i<4;++i) {
            int style=c->value[0]==letters[i]?t->style:GEAR_DIM_STYLE;
            const struct glyph *g=glyph(style,(unsigned int)letters[i]);
            bitmap_part(fonts[style].words,canvas,x,(t->height-g->h)/2,g->w,g->h,g->x,g->y);
            x+=g->advance+10;
        }
        return;
    }
    const unsigned char *s=(const unsigned char *)c->value;int width=0;
    while(*s)width+=glyph(t->style,next(&s))->advance;
    int x=t->center?(t->width-width)/2:0;
    s=(const unsigned char *)c->value;
    while(*s) {
        const struct glyph *g=glyph(t->style,next(&s));
        bitmap_part(fonts[t->style].words,canvas,x-3,(t->height-g->h)/2,g->w,g->h,g->x,g->y);
        x+=g->advance;
    }
}
static int hinge_group(int mask) {
    const int groups[]={1,2,4,8,16,32,63};
    for(int i=0;i<7;++i)if(groups[i]==mask)return i;
    return -1;
}
static void draw(void *self,void *canvas) {
    base_draw(self,canvas);
    for(int i=0;i<TYPE_LABELS+1;++i) {
        struct paint_control *c=&controls[i];if(c->p!=self)continue;
        if(c->type>=0)draw_text(c,canvas);
        else if(ready) {
            struct bitmap *b=&camera[current<0?0:current];int index=current<0?0:current;
            if((current==0 || current==64) && body_index>=0) {
                int frame=(int)(hinge_position*12+.5f);
                int group=hinge_group(hinge_mask);
                if(group>=0 && frame>0 && frame<12 && hinges[hinge_view*77+group*11+frame-1].loaded) {
                    index=193+hinge_view*77+group*11+frame-1;b=&hinges[hinge_view*77+group*11+frame-1];
                } else if(frame>=12 && body.loaded) {index=body_index;b=&body;}
            }
            const struct vehicle_sprite *v=&vehicle_sprites[index];
            if(b->loaded)bitmap_fill(b->words,canvas,v->x,v->y,v->w,v->h);
        }
        c->queued=0;return;
    }
}
int visual_bind(const struct dashborne_host *h) {
    host=h;
#define BIND(v,s) do {*(void **)(&(v))=h->resolve(h->ui_runtime,s);if(!(v)){h->log(h,"tesla-ui: visual missing " s);return -1;}}while(0)
    BIND(original,"_ZTV10ZKTextView");BIND(base_draw,"_ZN10ZKTextView6onDrawEP8ZKCanvas");
    BIND(invalidate,"_ZN6ZKBase10invalidateEPK14LayoutPosition");
    BIND(bitmap_ctor,"_ZN13BitmapWrapperC1Ev");BIND(bitmap_dtor,"_ZN13BitmapWrapperD1Ev");
    BIND(bitmap_load,"_ZN13BitmapWrapper18loadBitmapFromFileEPKcii");
    BIND(bitmap_fill,"_ZN13BitmapWrapper17fillBoxWithBitmapEPviiii");
    BIND(bitmap_part,"_ZN13BitmapWrapper21fillBoxWithBitmapPartEPviiiiii");
    BIND(clear_text,"_ZN10ZKTextView7setTextEPKc");BIND(set_picture,"_ZN6ZKBase16setBackgroundPicEPKc");
#undef BIND
    if(original[10]!=(void *)base_draw)return -1;
    for(int i=0;i<13;++i)table[i]=original[i];table[10]=(void *)draw;return 0;
}
int visual_attach(int id,void *p) {
    if(!p || *(void ***)p!=original+2)return 0;
    int type=-1,slot=TYPE_LABELS;
    if(id!=599306) {
        for(int i=0;i<TYPE_LABELS;++i)if(type_labels[i].id==id){type=i;slot=i;break;}
        if(type<0)return 0;
        int style=type_labels[type].style;
        if(!fonts[style].loaded) {
            char path[160];snprintf(path,sizeof(path),ASSETS "type/%d.png",style);
            if(!load(&fonts[style],path))return 0;
        }
        if(id==599320 && !fonts[GEAR_DIM_STYLE].loaded) {
            char path[160];snprintf(path,sizeof(path),ASSETS "type/%d.png",GEAR_DIM_STYLE);
            if(!load(&fonts[GEAR_DIM_STYLE],path))return 0;
        }
        clear_text(p,"");
    } else {
        for(int i=0;i<65;++i)if(!sprite_load(&camera[i],i)) {
            for(int j=0;j<=i;++j)release(&camera[j]);
            host->log(host,"tesla-ui: camera assets unavailable; using static car");return 0;
        }
        for(int i=0;i<154;++i)sprite_load(&hinges[i],193+i);
        ready=1;car_slot=slot;current=-1;motion=(struct camera_motion){0};
        set_picture(p,"");
    }
    controls[slot]=(struct paint_control){.p=p,.id=id,.type=type};
    *(void ***)p=table+2;
    if(type>=0)visual_text(p,type_labels[type].initial);
    return 1;
}
int visual_text(void *p,const char *value) {
    for(int i=0;i<TYPE_LABELS;++i)if(controls[i].p==p && p) {
        if(strcmp(controls[i].value,value)) {
            snprintf(controls[i].value,sizeof(controls[i].value),"%s",value);request(&controls[i]);
        }
        return 1;
    }
    return 0;
}
static void body_prepare(int drive,int mask) {
    dirty=1;body_view=drive;body_mask=mask;body_index=65+drive*64+mask;
    sprite_load(&body,body_index);
    hinge_mask=mask;hinge_view=drive;
}
int visual_vehicle(int drive,int mask,unsigned long long now,int immediate) {
    if(!ready)return 0;
    if(mask<0 || mask>63)mask=0;
    int initializing=!motion.primed;
    int changed_view=motion.primed && motion.target!=(drive?1:0);
    /* An open-panel snapshot stays accurate: no closed-car orbit over it. */
    camera_target(&motion,drive,now,immediate || mask || body_mask);
    if(immediate || changed_view || initializing) {
        /* Closed endpoints already live in the camera cache. Decoding another
           Park image here stalls the first tick of every gear transition. */
        if(mask && (!body.loaded || body_mask!=mask || body_view!=drive))body_prepare(drive,mask);
        if(!mask){body_mask=0;body_index=-1;body_view=drive;}
        hinge_position=hinge_from=mask?1:0;
        hinge_target=mask?1:0;hinge_start=now;requested_mask=mask;
    } else if(mask!=requested_mask) {
        requested_mask=mask;
        if(hinge_position==0 && mask)body_prepare(drive,mask);
        hinge_from=hinge_position;hinge_target=mask==body_mask && mask?1:0;hinge_start=now;
        if(!body_mask && mask){body_prepare(drive,mask);hinge_target=1;}
    }
    visual_frame(now);return 1;
}
void visual_frame(unsigned long long now) {
    if(!ready)return;
    int frame=camera_frame(&motion,now),old_hinge=(int)(hinge_position*12+.5f);
    float t=now>=hinge_start?(float)(now-hinge_start)/450000.0f:1;
    if(t>1)t=1;
    t=t*t*(3-2*t);
    hinge_position=hinge_from+(hinge_target-hinge_from)*t;
    if(hinge_position==0 && !requested_mask) {body_mask=0;body_index=-1;}
    if(hinge_position==0 && requested_mask && requested_mask!=body_mask) {
        body_prepare(body_view,requested_mask);hinge_from=0;hinge_target=1;hinge_start=now;
    }
    if(frame!=current || old_hinge!=(int)(hinge_position*12+.5f)) {
        current=frame;dirty=1;
    }
    if(dirty) {
        if(!last_frame || now>=last_frame || last_frame-now>16667) {
            request(&controls[car_slot]);
            last_frame=last_frame && now>=last_frame && now-last_frame<16667?last_frame+16667:now+16667;dirty=0;
        }
    }
}
void visual_detach(void) {
    for(int i=0;i<TYPE_LABELS+1;++i) {
        if(controls[i].p && *(void ***)controls[i].p==table+2)*(void ***)controls[i].p=original+2;
        controls[i]=(struct paint_control){0};
    }
    for(int i=0;i<TYPE_STYLES;++i)release(&fonts[i]);
    for(int i=0;i<65;++i)release(&camera[i]);
    for(int i=0;i<154;++i)release(&hinges[i]);release(&body);
    ready=0;car_slot=-1;body_index=-1;body_mask=0;requested_mask=0;last_frame=0;
}
