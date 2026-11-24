#include "runtime.h"
#include "memory.h"
extern int snprintf(char *,usize,const char *,...);
extern void *memcpy(void *,const void *,usize),*memset(void *,int,usize);
extern void *mmap(void *,usize,int,int,int,long);
extern int mprotect(void *,usize,int),munmap(void *,usize);
extern long syscall(long,...);
struct image_info {const char *name;void *base;const char *symbol;void *address;};
extern int dladdr(const void *,struct image_info *);
struct rect {int x,y,w,h;};
#define VIEW_COUNT 36
/* Independent IWidgetBase instances: no native gauge/video pool is borrowed. */
struct view {
    union {double align;unsigned char bytes[80];} native;
    struct dashborne_widget_view public;
    void *grid,*button;
    const void *last_theme;
    const struct widget_provider *provider,*attached_provider;
    unsigned int generation,attached_generation;
    unsigned int source;
    int initialized,attached,failed;
};
static struct view views[VIEW_COUNT];
static int paused=1,poisoned;
static void *activity,*code;
static struct memory_edit hooks[8];
static unsigned int hook_count;
static void *native_table[25],*draw_table[13],**text_table;
static int (*stock_count)(void *,int);
static void *(*factory)(void *,const struct layout *,int),*(*button_lookup)(void *,int);
static void (*grid_deactivate)(void *,unsigned short);
static void (*base_ctor)(void *),(*set_ui)(void *,const void *),(*set_editable)(void *,int),(*grid_activate)(void *,void *,const struct layout *);
static void *(*parent)(void *),*(*find)(void *,int),*(*theme)(void);
static int (*is_visible)(void *),(*get_id)(void *);
static void (*visible)(void *,int),(*text)(void *,const char *),(*listener)(void *,void *),(*set_position)(void *,const struct rect *);
static void (*set_alpha)(void *,unsigned char),(*button_picture)(void *,int,const char *),(*text_color)(void *,int,unsigned int),(*text_size)(void *,unsigned int);
static void (*json_ctor)(void *,int),(*json_dtor)(void *),(*reader_ctor)(void *),(*reader_dtor)(void *);
static int (*json_parse)(void *,const char *,const char *,void *,int);
static void (*parse_controls)(void *,const void *),(*invalidate)(void *,const struct rect *);
static void (*base_draw)(void *,void *),(*cpp_delete)(void *);
static void *pointer(const void *p){void *v;memcpy(&v,p,sizeof(v));return v;}
static struct view *native_view(void *p) {
    for(int i=0;i<VIEW_COUNT;++i)if(p==views[i].native.bytes)return &views[i];return 0;
}
static int noop(void *p,...) {(void)p;return 0;}
static void apply_theme(void *p,const void *config) {
    struct view *v=native_view(p);if(!v || !v->public.window || !config)return;
    /* Match the stock value/combined frames for the three small sizes and
       the projection frame geometry for 2x2 through 6x2, without its logic. */
    int offset=v->public.height==224?(v->public.width==250?0x5c:0x7c):
        v->public.width==250?0x6c:0x1a4+(v->public.width-510)/260*12;
    int invalid=offset<0x100?12:8;
    const unsigned char *c=config;
    button_picture(v->button,0,pointer(c+offset));
    button_picture(v->button,1,pointer(c+offset+4));
    button_picture(v->button,4,pointer(c+offset+invalid));
    unsigned int primary,secondary,selected;memcpy(&primary,c+0x20c,4);
    memcpy(&selected,c+0x210,4);memcpy(&secondary,c+0x214,4);
    text_color(v->public.value,0,primary);text_color(v->public.value,1,selected);
    text_color(v->public.title,0,secondary);text_color(v->public.status,0,secondary);
    v->last_theme=config;
}
static void alpha(void *p,int value) {
    struct view *v=native_view(p);if(v && v->public.window)set_alpha(v->public.window,(unsigned char)value);
}
static void render(void *self,void *surface) {
    base_draw(self,surface);
    if(paused)return;
    for(int i=0;i<VIEW_COUNT;++i) {
        struct view *v=&views[i];
        if(v->public.canvas==self && v->attached && v->attached_provider && v->attached_generation==v->generation && !v->failed && v->attached_provider->widget->draw) {
            v->attached_provider->widget->draw(&v->public,surface);return;
        }
    }
}
static const char document_format[]=
 "{\"window__widget\":{\"id\":%d,\"visible\":false,\"touchable\":false,\"backgroundColor\":657930,"
 "\"position\":{\"left\":0,\"top\":0,\"width\":250,\"height\":224},"
 "\"button__native\":{\"id\":%d,\"touchable\":false,\"position\":{\"left\":0,\"top\":0,\"width\":250,\"height\":224}},"
 "\"textview__canvas\":{\"id\":%d,\"touchable\":false,\"position\":{\"left\":12,\"top\":54,\"width\":226,\"height\":116}},"
 "\"textview__title\":{\"id\":%d,\"fontSize\":22,\"touchable\":false,\"colorTab\":{\"color0\":14738154}},"
 "\"textview__value\":{\"id\":%d,\"fontSize\":46,\"alignment\":37,\"touchable\":false,\"colorTab\":{\"color0\":14738154}},"
 "\"textview__status\":{\"id\":%d,\"fontSize\":19,\"alignment\":37,\"touchable\":false,\"colorTab\":{\"color0\":10068407}},"
 "\"button__action\":{\"id\":%d,\"fontSize\":18,\"alignment\":37,\"visible\":false,\"touchable\":true,"
 "\"backgroundColor\":2237484,\"colorTab\":{\"color0\":14738154}}}}";
static void release_provider(struct view *v) {
    if(v->attached && v->attached_provider && v->attached_provider->widget->destroy)v->attached_provider->widget->destroy(&v->public);
    v->attached=0;v->public.user=0;v->attached_provider=0;v->failed=0;
}
static int create_controls(struct view *v,int index,void *grid) {
    /* A stock 1x1 window identifies this grid's moving page parent. */
    void *stock_window=pointer((unsigned char *)grid+36+4);
    void *container=stock_window?parent(stock_window):0;
    if(!container)return -1;
    int id=610000+index*16;
    if(find(container,id))return -1;
    char document[2048];int n=snprintf(document,sizeof(document),document_format,id,id+1,id+2,id+3,id+4,id+5,id+6);
    if(n<0 || (usize)n>=sizeof(document))return -1;
    union {double align;unsigned char bytes[256];} reader;
    union {double align;unsigned char bytes[64];} json;
    reader_ctor(&reader);json_ctor(&json,0);
    int ok=json_parse(&reader,document,document+n,&json,0);
    if(ok)parse_controls(container,&json);
    json_dtor(&json);reader_dtor(&reader);
    v->public=(struct dashborne_widget_view){.struct_size=sizeof(v->public),.instance=(unsigned int)index,
        .window=find(container,id),.canvas=find(container,id+2),.title=find(container,id+3),
        .value=find(container,id+4),.status=find(container,id+5),.action=find(container,id+6)};
    v->button=find(container,id+1);v->grid=grid;
    if(!ok || !v->public.window || !v->button || !v->public.canvas || !v->public.title ||
       !v->public.value || !v->public.status || !v->public.action || *(void ***)v->public.canvas!=text_table+2)return -1;
    base_ctor(v->native.bytes);*(void ***)v->native.bytes=native_table+2;
    void *ui[2]={v->public.window,v->button};set_ui(v->native.bytes,ui);
    *(void ***)v->public.canvas=draw_table+2;
    listener(v->public.action,activity);v->initialized=1;return 0;
}
static void dimensions(struct view *v,const struct widget_geometry *g) {
    int w=g->width,h=g->height;v->last_theme=0;
    v->public.width=w;v->public.height=h;
    set_position(v->public.window,&(struct rect){0,0,w,h});
    set_position(v->button,&(struct rect){0,0,w,h});
    set_position(v->public.canvas,&(struct rect){12,54,w-24,h-108});
    int action_width=w==250?64:108;
    set_position(v->public.title,&(struct rect){16,12,w-action_width-48,36});
    text_size(v->public.title,w==250?16:22);text_size(v->public.status,w==250?16:19);
    set_position(v->public.value,&(struct rect){12,54,w-24,h-108});
    set_position(v->public.status,&(struct rect){12,h-48,w-24,36});
    set_position(v->public.action,&(struct rect){w-action_width-16,12,action_width,36});
    text(v->public.title,"");text(v->public.value,"");text(v->public.status,"");text(v->public.action,"");
    visible(v->public.action,0);
}
static void *activate_widget(void *grid,const struct layout *layout,int editing) {
    unsigned int source=layout && layout->begin && layout->end>layout->begin?layout->begin[0]:0;
    if(source<FIRST_SOURCE || source>LAST_SOURCE)return factory(grid,layout,editing);
    const struct widget_geometry *g=0;
    for(int s=0;s<DBW_SIZE_COUNT;++s)if(widget_sizes[s].type==layout->type)g=&widget_sizes[s];
    /* Older car placements borrowed any of the four 2x2 outlooks. */
    if(layout->type>=5 && layout->type<=8)g=&widget_sizes[DBW_2X2];
    if(!g || layout->x<0 || layout->y<0 || layout->x+g->columns>6 || layout->y+g->rows>2)return 0;
    for(int i=0;i<VIEW_COUNT;++i) {
        struct view *v=&views[i];
        if(v->grid && (v->grid!=grid || (v->public.window && is_visible(v->public.window))))continue;
        if(!v->initialized && create_controls(v,i,grid))return 0;
        ++v->generation;dimensions(v,g);v->source=source;v->provider=provider_for(source);
        text(v->public.title,v->provider?v->provider->widget->title:"Unavailable widget");
        if(!v->provider)text(v->public.status,"Enable its extension");
        set_editable(v->native.bytes,editing);grid_activate(grid,v->native.bytes,layout);
        apply_theme(v->native.bytes,theme());
        return v->native.bytes;
    }
    return 0;
}
static void *get_by_button(void *grid,int id) {
    for(int i=0;i<VIEW_COUNT;++i)if(views[i].grid==grid && views[i].initialized &&
        get_id(views[i].button)==id && is_visible(views[i].public.window))return views[i].native.bytes;
    return button_lookup(grid,id);
}
static void *video_layout(void *grid) {
    void **begin=pointer((unsigned char *)grid+24),**end=pointer((unsigned char *)grid+24+sizeof(void *));
    for(void **i=begin;i!=end;++i)if(*i && !native_view(*i)) {
        int type;memcpy(&type,(unsigned char *)*i+36,4);
        if(type>=12 && type<=16)return *i;
    }
    return 0;
}
static int count_stock(void *edit,int type) {
    int count=stock_count(edit,type);
    void *grid=pointer((unsigned char *)edit+48);if(!grid)return count;
    void **begin=pointer((unsigned char *)grid+24),**end=pointer((unsigned char *)grid+24+sizeof(void *));
    for(void **i=begin;i!=end;++i)if(native_view(*i)) {
        int actual;memcpy(&actual,(unsigned char *)*i+36,4);if(actual==type && count>0)--count;
    }
    return count;
}
/* Only installed before dashboard creation, and removed after its destruction.
   Two original position-independent instructions form each ARM trampoline. */
static int hook_entry(unsigned char *at,unsigned int first,unsigned int second,void *replacement,unsigned int *trampoline) {
    trampoline[0]=first;trampoline[1]=second;trampoline[2]=0xe51ff004u;trampoline[3]=(unsigned int)(uintptr)(at+8);
    unsigned int jump=0xe51ff004u,target=(unsigned int)(uintptr)replacement;
    if(memory_prepare(&hooks[hook_count],at+4,&second,&target,4))return -1;++hook_count;
    if(memory_prepare(&hooks[hook_count],at,&first,&jump,4))return -1;++hook_count;
    return 0;
}
int views_bind(const struct dashborne_host *h) {
#define BIND(v,lib,s) do {*(void **)(&(v))=h->resolve(lib,s);if(!(v)){h->log(h,"widgets: missing " s);return -1;}}while(0)
#define APP(v,s) BIND(v,h->official_library,s)
#define UI(v,s) BIND(v,h->ui_runtime,s)
    void **base_table;
    APP(grid_deactivate,"_ZN11DisplayGrid16deactivateWidgetEt");
    APP(base_table,"_ZTV11IWidgetBase");APP(base_ctor,"_ZN11IWidgetBaseC1Ev");
    APP(set_ui,"_ZN11IWidgetBase5setUIERK12WidgetBaseUI");APP(set_editable,"_ZN11IWidgetBase11setEditableEb");
    APP(grid_activate,"_ZN11DisplayGrid8activateER11IWidgetBaseRK9WdgLayout");
    APP(theme,"_ZN12ThemeManager9getActiveEv");
    UI(parent,"_ZNK6ZKBase9getParentEv");UI(find,"_ZN8ZKWindow15findControlByIDEi");
    UI(get_id,"_ZNK6ZKBase5getIDEv");UI(is_visible,"_ZNK6ZKBase9isVisibleEv");UI(visible,"_ZN6ZKBase10setVisibleEb");
    UI(text,"_ZN10ZKTextView7setTextEPKc");UI(listener,"_ZN6ZKBase16setClickListenerEPNS_14IClickListenerE");
    UI(set_position,"_ZN6ZKBase11setPositionERK14LayoutPosition");UI(set_alpha,"_ZN6ZKBase8setAlphaEh");
    UI(button_picture,"_ZN8ZKButton18setButtonStatusPicEiPKc");
    UI(text_size,"_ZN10ZKTextView11setTextSizeEj");
    UI(text_color,"_ZN10ZKTextView18setTextStatusColorEij");
    UI(json_ctor,"_ZN4Json5ValueC1ENS_9ValueTypeE");UI(json_dtor,"_ZN4Json5ValueD1Ev");
    UI(reader_ctor,"_ZN4Json6ReaderC1Ev");UI(reader_dtor,"_ZN4Json6ReaderD2Ev");UI(json_parse,"_ZN4Json6Reader5parseEPKcS2_RNS_5ValueEb");
    UI(parse_controls,"_ZN8ZKWindow21parseControlsFromJsonERKN4Json5ValueE");
    UI(text_table,"_ZTV10ZKTextView");UI(base_draw,"_ZN10ZKTextView6onDrawEP8ZKCanvas");UI(invalidate,"_ZN6ZKBase10invalidateEPK14LayoutPosition");
#undef UI
#undef APP
#undef BIND
    struct image_info image;
    h->log(h,"widgets: view symbols resolved");
    if(sizeof(void *)!=4 || !dladdr(base_table,&image) || base_table!=(void **)((unsigned char *)image.base+0x351e10) ||
       text_table[10]!=(void *)base_draw)return -1;
    /* Use the app's allocator pair through its verified delete PLT entry. */
    cpp_delete=(void (*)(void *))((unsigned char *)image.base+0x9893c);
    for(int i=0;i<25;++i)native_table[i]=base_table[i];
    for(int i=0;i<23;++i)if(i!=12 && i!=19)native_table[i+2]=(void *)noop;
    native_table[12]=(void *)apply_theme;native_table[22]=(void *)alpha;
    for(int i=0;i<13;++i)draw_table[i]=text_table[i];draw_table[10]=(void *)render;
    h->log(h,"widgets: view tables verified");
    code=mmap(0,4096,3,0x22,-1,0);if(code==(void *)-1){code=0;return -1;}
    unsigned char *base=image.base;
    if(hook_entry(base+0x11c014,0xe92d47f0u,0xe1a07001u,(void *)activate_widget,code) ||
       hook_entry(base+0x11b26c,0xe92d47f0u,0xe2809e56u,(void *)get_by_button,(unsigned int *)code+4) ||
       hook_entry(base+0x113c4c,0xe5902018u,0xe590101cu,(void *)video_layout,(unsigned int *)code+8) ||
       hook_entry(base+0xf8d58,0xe92d41f0u,0xe1a08000u,(void *)count_stock,(unsigned int *)code+12))return -1;
    h->log(h,"widgets: entry hooks prepared");
    if(syscall(0xf0002L,(uintptr)code,(uintptr)code+64,0L) || mprotect(code,4096,5))return -1;
    *(void **)(&factory)=code;*(void **)(&button_lookup)=(unsigned int *)code+4;
    *(void **)(&stock_count)=(unsigned int *)code+12;
    for(unsigned int i=0;i<hook_count;++i)if(memory_apply(&hooks[i]))return -1;
    h->log(h,"widgets: custom instance factory ready");
    return 0;
}
void views_activity(void *self){activity=self;}
void views_pause(int value){
    if(value && !paused)for(int i=0;i<VIEW_COUNT;++i) {
        struct view *v=&views[i];
        if(v->attached && v->attached_provider && v->attached_provider->widget->suspend)
            v->attached_provider->widget->suspend(&v->public);
    }
    paused=value;
}
void views_tick(unsigned long long now) {
    if(paused)return;
    const void *active_theme=theme();
    for(int i=0;i<VIEW_COUNT;++i) {
        struct view *v=&views[i];if(!v->initialized)continue;
        if(v->last_theme!=active_theme)apply_theme(v->native.bytes,active_theme);
        v->public.editing=v->native.bytes[0x14]!=0;
        void *container=parent(v->public.window);
        v->public.visible=is_visible(v->public.window) && container && is_visible(container);
        if(!is_visible(v->public.window)){release_provider(v);continue;}
        if(v->attached && v->attached_generation!=v->generation)release_provider(v);
        if(!v->provider || v->failed)continue;
        if(!v->attached) {
            v->attached=1;v->attached_provider=v->provider;v->attached_generation=v->generation;
            if(v->provider->widget->create && v->provider->widget->create(&v->public)) {
                v->failed=1;text(v->public.status,"Widget could not start");continue;
            }
        }
        if(v->provider->widget->update)v->provider->widget->update(&v->public,now);
        visible(v->public.action,!v->public.editing && v->provider->widget->click!=0);
    }
}
void views_text(void *control,const char *value){if(text && control)text(control,value);}
void views_invalidate(struct dashborne_widget_view *v){if(v && v->canvas)invalidate(v->canvas,0);}
int views_click(void *control) {
    for(int i=0;i<VIEW_COUNT;++i) {
        struct view *v=&views[i];
        if(control==v->public.action && v->attached && v->attached_generation==v->generation && v->provider && !paused && !v->native.bytes[0x14] &&
           is_visible(v->public.window) && v->provider->widget->click){v->provider->widget->click(&v->public);return 1;}
    }
    return 0;
}
int views_detach(void) {
    for(int i=0;i<VIEW_COUNT;++i) {
        struct view *v=&views[i];
        if(v->initialized) {unsigned short id;memcpy(&id,v->native.bytes+48,2);grid_deactivate(v->grid,id);}
        release_provider(v);
        if(v->public.canvas) {
            if(*(void ***)v->public.canvas==draw_table+2)*(void ***)v->public.canvas=text_table+2;
            else if(*(void ***)v->public.canvas!=text_table+2)poisoned=1;
        }
        if(v->public.action)listener(v->public.action,0);
        if(v->public.window)visible(v->public.window,0);
        if(v->initialized) {void *allocation=pointer(v->native.bytes+52);if(allocation)cpp_delete(allocation);}
        *v=(struct view){0};
    }
    return poisoned?-1:0;
}
int views_cleanup(void) {
    if(poisoned)return -1;
    for(unsigned int i=hook_count;i>0;--i)if(memory_restore(&hooks[i-1]))return -1;
    hook_count=0;if(code){munmap(code,4096);code=0;}return 0;
}

#ifdef DASHBORNE_WIDGET_DEVICE_TEST
#include "tests/device_probe.h"
void widget_device_probe(unsigned long long now){views_device_probe(now);}
#endif
