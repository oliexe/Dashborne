#include "views.h"
#include "registry.h"
#include "memory.h"
typedef __SIZE_TYPE__ usize;
typedef __UINTPTR_TYPE__ uintptr;
extern int snprintf(char *,usize,const char *,...),strcmp(const char *,const char *);
extern usize strlen(const char *);
extern void *memcpy(void *,const void *,usize);
struct image_info {const char *name;void *base;const char *symbol;void *address;};
extern int dladdr(const void *,struct image_info *);
struct rect {int x,y,w,h;};
/* Three native page holders, each with four reusable 2x2 gauge containers. */
#define VIEW_COUNT 12
struct view {void *widget,*parent,*window,*canvas,*status,*button;int selected,editing;};
static struct view views[VIEW_COUNT];
static unsigned int count;
static void *(*book)(void),*(*find)(void *,int);
static const struct rect *(*position)(void *);
static int (*is_visible)(void *);
static void (*visible)(void *,int),(*text)(void *,const char *),(*listener)(void *,void *);
static void (*json_ctor)(void *,int),(*json_dtor)(void *),(*reader_ctor)(void *),(*reader_dtor)(void *);
static int (*json_parse)(void *,const char *,const char *,void *,int);
static void (*parse_controls)(void *,const void *),(*invalidate)(void *,const struct rect *);
static void (*base_draw)(void *,void *),(*render)(void *);
static void **text_table,*draw_table[13];
static struct memory_edit visibility_edit;
static int preview;
static char previous_status[100];
static void *pointer(const void *p){void *v;memcpy(&v,p,sizeof(v));return v;}
static const char layout[]=
    "{\"window__vehicle\":{\"id\":%d,\"visible\":false,\"touchable\":false,\"backgroundColor\":657930,"
    "\"position\":{\"left\":0,\"top\":0,\"width\":510,\"height\":458},"
    "\"textview__title\":{\"id\":%d,\"text\":\"3D Vehicle\",\"fontSize\":22,\"touchable\":false,"
    "\"colorTab\":{\"color0\":14738154},\"position\":{\"left\":18,\"top\":16,\"width\":300,\"height\":40}},"
    "\"textview__car\":{\"id\":%d,\"touchable\":false,\"position\":{\"left\":15,\"top\":78,\"width\":480,\"height\":288}},"
    "\"textview__status\":{\"id\":%d,\"fontSize\":19,\"touchable\":false,\"alignment\":37,"
    "\"colorTab\":{\"color0\":10068407},\"position\":{\"left\":18,\"top\":389,\"width\":474,\"height\":44}},"
    "\"button__preview\":{\"id\":%d,\"text\":\"Preview\",\"fontSize\":18,\"alignment\":37,\"touchable\":true,"
    "\"backgroundColor\":2237484,\"colorTab\":{\"color0\":14738154},"
    "\"position\":{\"left\":380,\"top\":16,\"width\":112,\"height\":42}}}}";
static void draw(void *self,void *surface) {
    base_draw(self,surface);
    for(unsigned int i=0;i<count;++i)if(views[i].canvas==self && views[i].selected) {render(surface);return;}
}
static void select_view(struct view *v) {
    int selected=registry_widget(v->widget);
    int editing=((unsigned char *)v->widget)[0x14]!=0;
    if(selected!=v->selected) {v->selected=selected;visible(v->window,selected);}
    if(editing!=v->editing) {v->editing=editing;visible(v->button,!editing);}
}
/* Synchronize before the native carrier becomes visible, not on a later timer.
   A recycled container must never show either its old car or its stock gauge. */
static void native_visible(void *control,int value) {
    for(unsigned int i=0;i<count;++i)if(views[i].parent==control) {select_view(&views[i]);break;}
    visible(control,value);
}
int views_bind(const struct dashborne_host *h,void (*paint)(void *)) {
    render=paint;
#define BIND(v,lib,s) do {*(void **)(&(v))=h->resolve(lib,s);if(!(v))return -1;}while(0)
#define UI(v,s) BIND(v,h->ui_runtime,s)
    BIND(book,h->official_library,"_ZN10WidgetBook8instanceEv");
    UI(find,"_ZN8ZKWindow15findControlByIDEi");UI(position,"_ZNK6ZKBase11getPositionEv");
    UI(is_visible,"_ZNK6ZKBase9isVisibleEv");UI(visible,"_ZN6ZKBase10setVisibleEb");
    UI(text,"_ZN10ZKTextView7setTextEPKc");UI(listener,"_ZN6ZKBase16setClickListenerEPNS_14IClickListenerE");
    UI(json_ctor,"_ZN4Json5ValueC1ENS_9ValueTypeE");UI(json_dtor,"_ZN4Json5ValueD1Ev");
    UI(reader_ctor,"_ZN4Json6ReaderC1Ev");UI(reader_dtor,"_ZN4Json6ReaderD2Ev");
    UI(json_parse,"_ZN4Json6Reader5parseEPKcS2_RNS_5ValueEb");
    UI(parse_controls,"_ZN8ZKWindow21parseControlsFromJsonERKN4Json5ValueE");
    UI(text_table,"_ZTV10ZKTextView");UI(base_draw,"_ZN10ZKTextView6onDrawEP8ZKCanvas");
    UI(invalidate,"_ZN6ZKBase10invalidateEPK14LayoutPosition");
#undef UI
#undef BIND
    struct image_info image;
    if(!paint || sizeof(void *)!=4 || !dladdr((void *)book,&image) ||
       (uintptr)book!=(uintptr)image.base+0xf36dc || text_table[10]!=(void *)base_draw)return -1;
    for(int i=0;i<13;++i)draw_table[i]=text_table[i];draw_table[10]=(void *)draw;
    void *slot=(unsigned char *)image.base+0x358414;
    void *before=pointer(slot),*after=(void *)native_visible;
    if(before!=(void *)visible && before!=(unsigned char *)image.base+0x97b78)return -1;
    return memory_prepare(&visibility_edit,slot,&before,&after,4) || memory_apply(&visibility_edit)?-1:0;
}
int views_create(void *activity) {
    if(count)return -1;
    previous_status[0]=0;preview=0;
    const unsigned int offsets[]={4044,4384,4724,4996};
    unsigned char *owner=book();if(!owner)return -1;
    for(unsigned int page=0;page<3;++page) {
        unsigned char *grid=pointer(owner+0x64+page*12);if(!grid)return -1;
        for(unsigned int gauge=0;gauge<4;++gauge) {
            unsigned int i=page*4+gauge;
            struct view *v=&views[i];*v=(struct view){.widget=grid+offsets[gauge],.selected=-1,.editing=-1};
            v->parent=pointer((unsigned char *)v->widget+4);
            const struct rect *p=v->parent?position(v->parent):0;
            if(!p || p->w!=510 || p->h!=458 || find(v->parent,119700+(int)i))return -1;
            char document[2048];
            int n=snprintf(document,sizeof(document),layout,119700+i,599700+i*10,599701+i*10,599702+i*10,299700+i);
            if(n<0 || (usize)n>=sizeof(document))return -1;
            union {double align;unsigned char bytes[256];} reader;
            union {double align;unsigned char bytes[64];} json;
            reader_ctor(&reader);json_ctor(&json,0);
            int ok=json_parse(&reader,document,document+n,&json,0);
            if(ok)parse_controls(v->parent,&json);
            json_dtor(&json);reader_dtor(&reader);
            v->window=find(v->parent,119700+i);v->canvas=find(v->parent,599701+i*10);
            v->status=find(v->parent,599702+i*10);v->button=find(v->parent,299700+i);
            /* Publish partial ownership too, so failed creation can detach. */
            count=i+1;
            if(!ok || !v->window || !v->canvas || !v->status || !v->button || *(void ***)v->canvas!=text_table+2)return -1;
            *(void ***)v->canvas=draw_table+2;listener(v->button,activity);select_view(v);
        }
    }
    return 0;
}
int views_sync(void) {
    int selected=0;
    for(unsigned int i=0;i<count;++i) {select_view(&views[i]);selected+=views[i].selected && is_visible(views[i].parent);}
    return selected;
}
int views_editing(void) {
    for(unsigned int i=0;i<count;++i)if(views[i].selected && views[i].editing && is_visible(views[i].parent))return 1;
    return 0;
}
void views_invalidate(void) {
    for(unsigned int i=0;i<count;++i)if(views[i].selected && is_visible(views[i].parent))invalidate(views[i].canvas,0);
}
void views_label(const char *value) {
    if(!strcmp(previous_status,value))return;
    for(unsigned int i=0;i<count;++i)if(views[i].status)text(views[i].status,value);
    snprintf(previous_status,sizeof(previous_status),"%s",value);
}
void views_preview(int value) {
    if(value==preview)return;preview=value;
    for(unsigned int i=0;i<count;++i)if(views[i].button)text(views[i].button,value?"Stop":"Preview");
}
int views_click(void *control) {
    for(unsigned int i=0;i<count;++i)if(views[i].button==control) {
        select_view(&views[i]);return views[i].selected && !views[i].editing && is_visible(views[i].parent);
    }
    return 0;
}
int views_detach(void) {
    for(unsigned int i=0;i<count;++i)if(views[i].canvas && *(void ***)views[i].canvas!=draw_table+2 && *(void ***)views[i].canvas!=text_table+2)return -1;
    unsigned int n=count;count=0;
    for(unsigned int i=0;i<n;++i) {
        struct view *v=&views[i];
        if(v->window)visible(v->window,0);
        if(v->canvas)*(void ***)v->canvas=text_table+2;
        if(v->button)listener(v->button,0);
        *v=(struct view){0};
    }
    return 0;
}
int views_cleanup(void) {if(views_detach())return -1;return memory_restore(&visibility_edit);}
