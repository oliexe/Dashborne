#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../views.c"
struct control {void **table;int shown,id;struct control *parent;struct rect rect;};
static unsigned char owner[256],grids[3][5400];
static struct control carriers[12],children[12][4];
static void *native_text[13];
static int selected[12],created,painted,invalidations,labels,creation_failure=-1;
static const unsigned int offsets[]={4044,4384,4724,4996};
static void assign(void *at,void *value){memcpy(at,&value,sizeof(value));}
static void *widget(int i){return grids[i/4]+offsets[i%4];}
int registry_widget(const void *w){for(int i=0;i<12;++i)if(w==widget(i))return selected[i];return 0;}
int memory_prepare(struct memory_edit *e,void *p,const void *a,const void *b,unsigned int n){(void)e;(void)p;(void)a;(void)b;(void)n;return 0;}
int memory_apply(struct memory_edit *e){(void)e;return 0;}
int memory_restore(struct memory_edit *e){(void)e;return 0;}
static void *get_book(void){return owner;}
static const struct rect *get_position(void *p){return &((struct control *)p)->rect;}
static int get_visible(void *p){return ((struct control *)p)->shown;}
static void set_visible(void *p,int value){
    /* At the instant stock content is exposed, its car cover must be correct. */
    for(int i=0;i<12;++i)if(p==&carriers[i] && value && count==12)assert(children[i][0].shown==selected[i]);
    ((struct control *)p)->shown=value;
}
static void set_text(void *p,const char *s){assert(p && s);++labels;}
static void listen(void *p,void *a){assert(p && (!a || a==(void *)1));}
static void noop(void *p){(void)p;}
static void json_init(void *p,int type){(void)p;assert(!type);}
static int parse_json(void *r,const char *b,const char *e,void *v,int comments){
    (void)r;(void)v;assert(e-b==(long)strlen(b) && !comments);
    assert(strstr(b,"\"left\":0,\"top\":0,\"width\":510,\"height\":458"));
    return 1;
}
static void parse_tree(void *p,const void *json){
    assert(json && p==&carriers[created]);int i=created++;
    int ids[]={119700+i,599701+i*10,599702+i*10,299700+i};
    for(int j=0;j<4;++j)children[i][j]=(struct control){.table=native_text+2,.id=ids[j],.parent=p,.shown=j!=0};
}
static void *lookup(void *p,int id){
    for(int i=0;i<created;++i)if(p==&carriers[i])for(int j=0;j<4;++j)
        if(children[i][j].id==id && !(i==creation_failure && j==3))return &children[i][j];
    return 0;
}
static void native_draw(void *p,void *surface){assert(p && surface==(void *)2);}
static void paint(void *surface){assert(surface==(void *)2);++painted;}
static void invalid(void *p,const struct rect *r){assert(p && !r);++invalidations;}
static void setup(void){
    created=count=0;memset(selected,0,sizeof(selected));memset(views,0,sizeof(views));memset(children,0,sizeof(children));
    for(int i=0;i<3;++i)assign(owner+0x64+i*12,grids[i]);
    for(int i=0;i<12;++i){carriers[i]=(struct control){.rect={1040,11,510,458}};assign((unsigned char *)widget(i)+4,&carriers[i]);((unsigned char *)widget(i))[0x14]=0;}
    book=get_book;position=get_position;is_visible=get_visible;visible=set_visible;text=set_text;listener=listen;
    reader_ctor=reader_dtor=json_dtor=noop;json_ctor=json_init;json_parse=parse_json;parse_controls=parse_tree;find=lookup;
    text_table=native_text;for(int i=0;i<13;++i)draw_table[i]=native_text[i];draw_table[10]=(void *)draw;
    base_draw=native_draw;render=paint;invalidate=invalid;
}
int main(void){
    setup();selected[0]=selected[4]=1;
    assert(!views_create((void *)1) && count==12 && created==12);
    for(int i=0;i<12;++i)assert(children[i][0].parent==&carriers[i] && children[i][0].shown==selected[i]);
    native_visible(&carriers[0],1);native_visible(&carriers[4],1);
    assert(views_sync()==2);int paints=painted;
    /* Both pages stay covered throughout a drag; coordinates belong to the
       parent and are never reset or used to hide the child view. */
    for(int step=0;step<=20;++step){
        carriers[0].rect.x=1040-step*78;carriers[4].rect.x=2590-step*78;
        assert(views_sync()==2 && children[0][0].shown && children[4][0].shown);
        draw(&children[0][1],(void *)2);draw(&children[4][1],(void *)2);
    }
    assert(painted==paints+42);views_invalidate();assert(invalidations==2);
    /* Reuse the same pooled window for a stock gauge, then for a car again.
       Synchronization must happen BEFORE native setVisible, without a tick. */
    native_visible(&carriers[0],0);selected[0]=0;native_visible(&carriers[0],1);
    assert(!children[0][0].shown && children[4][0].shown);
    selected[0]=1;native_visible(&carriers[0],1);assert(children[0][0].shown);
    ((unsigned char *)widget(0))[0x14]=1;views_sync();assert(views_editing() && !children[0][3].shown && !views_click(&children[0][3]));
    assert(children[0][0].shown);((unsigned char *)widget(0))[0x14]=0;views_sync();assert(views_click(&children[0][3]));
    views_label("Live: Park");int before=labels;views_label("Live: Park");assert(labels==before);
    views_preview(1);assert(labels==before+12);views_preview(1);assert(labels==before+12);
    children[0][1].table=0;assert(views_detach()==-1 && count==12);
    children[0][1].table=draw_table+2;assert(!views_detach() && count==0);
    for(int i=0;i<12;++i)assert(children[i][1].table==native_text+2 && !children[i][0].shown);
    /* Native controls also retire cleanly after partial parse failure. */
    setup();creation_failure=5;assert(views_create((void *)1)==-1 && count==6);assert(!views_cleanup());
    for(int i=0;i<6;++i)assert(children[i][1].table==native_text+2 && !children[i][0].shown);
    setup();creation_failure=-1;assert(!views_create((void *)1));assert(!views_cleanup());
    puts("Vehicle child views: native parenting, simultaneous page slides, recycling, editing and partial cleanup passed");
}
