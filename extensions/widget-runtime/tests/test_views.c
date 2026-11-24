#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../views.c"
struct widget_provider providers[PROVIDERS];
const struct widget_geometry widget_sizes[8]={{1,1,0,250,224},{2,1,2,510,224},{1,2,4,250,458},{2,2,5,510,458},{3,2,13,770,458},{4,2,14,1030,458},{5,2,15,1290,458},{6,2,16,1550,458}};
const struct widget_provider *provider_for(unsigned int s){return s==0x212?&providers[0]:0;}
struct control {void **table;int id,shown;struct rect rect,icon_rect;void *parent;char text[100];};
static struct control controls[36][7],pages[3],stock[3];
static unsigned char grids[3][80];
static int current,creates,destroys,updates,draws,clicks,delegations,lookup_calls,deactivations,parse_fail;
static void *tables[13];
static void *get_parent(void *p){return ((struct control *)p)->parent;}
static void *lookup(void *p,int id) {
    int i=(id-610000)/16,j=(id-610000)%16;
    if(i<0 || i>=36 || j>6 || controls[i][j].parent!=p)return 0;
    return &controls[i][j];
}
static void visibility(void *p,int b){((struct control *)p)->shown=b;}
static int shown(void *p){return ((struct control *)p)->shown;}
static int id(void *p){return ((struct control *)p)->id;}
static void caption(void *p,const char *s){snprintf(((struct control *)p)->text,100,"%s",s);}
static void font_size(void *p,unsigned int n){assert(p && n>=16 && n<=22);}
static int original_count(void *e,int t){(void)e;(void)t;return 2;}
static void geometry(void *p,const struct rect *r){((struct control *)p)->rect=*r;}
static void icon_geometry(void *p,const struct rect *r){((struct control *)p)->icon_rect=*r;}
static void listen(void *p,void *a){(void)p;(void)a;}
static void ctor(void *p,int n){(void)p;(void)n;}
static void reader_init(void *p){(void)p;}
static void dtor(void *p){(void)p;}
static int parse(void *r,const char *a,const char *b,void *j,int n){(void)r;(void)b;(void)j;(void)n;int v;assert(sscanf(strstr(a,"\"id\":"),"\"id\":%d",&v)==1);current=(v-610000)/16;return !parse_fail;}
static void controls_parse(void *p,const void *j){(void)j;for(int i=0;i<7;++i)controls[current][i]=(struct control){.table=tables+2,.id=610000+current*16+i,.parent=p};}
static void init_base(void *p){memset(p,0,80);}
static void ui_base(void *p,const void *ui){(void)p;(void)ui;}
static void editable(void *p,int e){((unsigned char *)p)[20]=e;}
static void activate_grid(void *g,void *p,const struct layout *l){struct view *v=native_view(p);assert(v && v->grid==g);memcpy(v->native.bytes+36,l,16);visibility(v->public.window,1);}
static void deactivate_grid(void *g,unsigned short i){(void)g;(void)i;++deactivations;}
static void *stock_factory(void *g,const struct layout *l,int e){(void)g;(void)l;(void)e;++delegations;return (void *)5;}
static void *stock_lookup(void *g,int n){(void)g;(void)n;++lookup_calls;return (void *)6;}
static void *get_theme(void){return 0;}
static void original_draw(void *c,void *s){(void)c;(void)s;}
static void dirty(void *c,const struct rect *r){(void)c;(void)r;}
static int on_create(struct dashborne_widget_view *v){assert(!v->user);v->user=(void *)7;++creates;return 0;}
static void on_destroy(struct dashborne_widget_view *v){assert(v->user==(void *)7);++destroys;}
static void on_update(struct dashborne_widget_view *v,unsigned long long n){(void)n;assert(v->user==(void *)7);++updates;}
static void on_draw(struct dashborne_widget_view *v,void *c){(void)c;assert(v->user==(void *)7);++draws;}
static void on_click(struct dashborne_widget_view *v){assert(v->user==(void *)7);++clicks;}
int memory_prepare(struct memory_edit *e,void *p,const void *a,const void *b,unsigned int n){(void)e;(void)p;(void)a;(void)b;assert(n==4);return 0;}
int memory_apply(struct memory_edit *e){(void)e;return 0;}
int memory_restore(struct memory_edit *e){(void)e;return 0;}
int main(void) {
    const struct dashborne_host h={0};
    const struct dashborne_widget w={.create=on_create,.destroy=on_destroy,.update=on_update,.draw=on_draw,.click=on_click,.title="Test"};
    providers[0]=(struct widget_provider){&h,&w,0x212};
    parent=get_parent;find=lookup;visible=visibility;is_visible=shown;get_id=id;text=caption;listener=listen;
    set_position=geometry;set_icon_position=icon_geometry;text_size=font_size;json_ctor=ctor;json_dtor=reader_dtor=dtor;reader_ctor=reader_init;json_parse=parse;parse_controls=controls_parse;
    base_ctor=init_base;set_ui=ui_base;set_editable=editable;grid_activate=activate_grid;grid_deactivate=deactivate_grid;cpp_delete=dtor;
    factory=stock_factory;button_lookup=stock_lookup;theme=get_theme;invalidate=dirty;base_draw=original_draw;text_table=tables;
    for(int i=0;i<3;++i){pages[i].shown=1;stock[i].parent=&pages[i];void *p=&stock[i];memcpy(grids[i]+40,&p,sizeof(p));}
    unsigned short source=0x212;struct layout l={.id=42,.begin=&source,.end=&source+1,.capacity=&source+1};
    views_activity((void *)9);views_pause(0);
    for(int size=0;size<8;++size) {
        l.type=widget_sizes[size].type;
        for(int page=0;page<3;++page) {
            void *p=activate_widget(grids[page],&l,0);assert(p);
            struct view *v=native_view(p);assert(parent(v->public.window)==&pages[page]);
            assert(v->public.width==widget_sizes[size].width && v->public.height==widget_sizes[size].height);
            assert(((struct control *)v->public.canvas)->rect.w==v->public.width-24);
            struct control *frame=v->button;
            assert(frame->icon_rect.x==0 && frame->icon_rect.y==0 && frame->icon_rect.w==v->public.width && frame->icon_rect.h==v->public.height);
            assert(get_by_button(grids[page],id(v->button))==p);
            /* A newly reassigned window cannot draw the retired provider. */
            int old_draws=draws;render(v->public.canvas,0);assert(draws==old_draws);
            views_tick(100000);render(v->public.canvas,0);assert(draws==old_draws+1);
            assert(views_click(v->public.action));v->native.bytes[20]=1;views_tick(200000);assert(!views_click(v->public.action));
            assert(!shown(v->public.action));v->native.bytes[20]=0;
        }
        for(int i=0;i<36;++i)if(views[i].initialized)visibility(views[i].public.window,0);
    }
    source=0x201;assert(activate_widget(grids[0],&l,0)==(void *)5 && delegations==1);
    assert(get_by_button(grids[0],99)==(void *)6 && lookup_calls==1);
    source=0x212;l.type=16;l.x=1;assert(!activate_widget(grids[0],&l,0));l.x=0;
    struct view *v=native_view(activate_widget(grids[0],&l,0));assert(v);views_tick(300000);
    unsigned char native_video[80]={0},edit[80]={0};int video_type=13;memcpy(native_video+36,&video_type,4);
    void *mix[]={v->native.bytes,native_video};void *begin=mix,*end=mix+2,*grid=grids[0];
    memcpy(grids[0]+24,&begin,sizeof(begin));memcpy(grids[0]+24+sizeof(void *),&end,sizeof(end));memcpy(edit+48,&grid,sizeof(grid));
    stock_count=original_count;assert(video_layout(grid)==native_video);assert(count_stock(edit,16)==1 && count_stock(edit,13)==2);
    int before=updates;views_pause(1);views_tick(400000);assert(updates==before && !views_click(v->public.action));
    views_pause(0);views_tick(500000);assert(updates>before);
    assert(!views_detach() && creates==destroys && deactivations==3);
    assert(clicks==24 && !views_cleanup());
    puts("Widget views: 8 sizes with matching frame bounds, three page parents, native delegation, independent instances, no stale frame, edit gating, pause and teardown passed");
}
