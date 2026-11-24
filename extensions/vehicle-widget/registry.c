#include "registry.h"
#include "memory.h"
#include "storage.h"
typedef __SIZE_TYPE__ usize;
typedef __UINTPTR_TYPE__ uintptr;
extern int snprintf(char *,usize,const char *,...),strcmp(const char *,const char *);
extern void *memcpy(void *,const void *,usize);
struct image_info {const char *name;void *base;const char *symbol;void *address;};
extern int dladdr(const void *,struct image_info *);
struct cell {int x,y;};
struct layout {int type,x,y;unsigned short id,padding;unsigned short *begin,*end,*capacity;};
struct option {int width,height;unsigned int source;const char *picture;};
#if defined(__arm__)
_Static_assert(sizeof(struct layout)==28,"ARM layout ABI");
_Static_assert(sizeof(struct option)==16,"ARM picker ABI");
#endif
static const struct dashborne_host *api;
static unsigned char *base;
static struct memory_edit edits[20];
static unsigned int edit_count;
static void *stock_special;
/* Word layout is the fingerprinted ARM WdgList record, copied only at runtime. */
static uintptr catalog[18][18];
#define descriptor catalog[17]
static struct option options[5];
static unsigned int category_ids[5]={1,2,3,8,17};
static char thumbnail[512];
static int *tab,*category;
static unsigned char *intent;
static void **caller;
static void (*original_click)(void *,int,int),(*original_data)(void *,void *,int);
static void (*picker_resume)(void *);
static int (*json_write)(const char *,const void *);
static void *(*find_sub)(const void *,int);
static void (*set_text)(void *,const char *);
static void *(*active_grid)(void *),*(*book)(void),*(*widgets)(void *),*(*loader)(void);
static int (*loader_load)(void *),(*count_on_page)(void *,int);
static int (*nearest)(void *,int,struct cell,struct cell *,unsigned short);
static void (*add_widget)(void *,struct layout *),(*remove_widget)(void *,unsigned short),(*close_editor)(void *,int);
static unsigned int registered;
static unsigned int path_edits;
static int persistence_busy;
static void *pointer(const void *p){void *v;memcpy(&v,p,sizeof(v));return v;}
static unsigned short short_at(const void *p){unsigned short v;memcpy(&v,p,sizeof(v));return v;}
int registry_widget(const void *w) {
    if(!w)return 0;
    const unsigned char *p=w;
    const unsigned short *begin=pointer(p+52),*end=pointer(p+52+sizeof(void *));
    uintptr a=(uintptr)begin,b=(uintptr)end;
    return a && b>=a && b-a>=2 && b-a<=8 && !((a|b)&1) && short_at(begin)==VEHICLE_SOURCE;
}
static void prepare_catalog(const uintptr *stock,void *refresh) {
    for(unsigned int i=0;i<17*18;++i)((uintptr *)catalog)[i]=stock[i];
    for(unsigned int i=0;i<18;++i)descriptor[i]=stock[18+i];
    descriptor[0]=(uintptr)"3D Vehicle";descriptor[1]=(uintptr)"3D Vehicle";
    descriptor[2]=descriptor[3]=(uintptr)"";
    descriptor[15]=(uintptr)refresh;descriptor[16]=0;
    descriptor[17]=(uintptr)"Animated vehicle state. Park, Drive, doors, frunk and trunk.";
}
static int mine(int row) {return *tab==2 && category[2]==1 && row==4;}
/* Stock placement owns occupancy and IDs. A single car per page shares one of
   the four native 2x2 gauge containers; no existing choice is replaced. */
static int candidate(struct layout *out) {
    void *edit=*caller;if(!edit || !intent[0] || intent[0]>25)return 0;
    void *grid=active_grid(book());if(!grid || pointer((unsigned char *)edit+48)!=grid)return 0;
    int x,y;memcpy(&x,intent+4,4);memcpy(&y,intent+8,4);
    if(x<0 || x>5 || y<0 || y>1)return 0;
    unsigned short replacing=short_at(intent+36);
    void **v=widgets(grid);if(!v)return 0;
    uintptr begin=(uintptr)v[0],end=(uintptr)v[1];
    if(end<begin || (end-begin)%sizeof(void *) || (end-begin)/sizeof(void *)>64)return 0;
    int old_type=-1;
    for(void **i=(void **)begin;(uintptr)i<end;++i) {
        if(!*i)return 0;
        unsigned short id=short_at((unsigned char *)*i+48);
        if(id==replacing)memcpy(&old_type,(unsigned char *)*i+36,4);
        if(registry_widget(*i) && id!=replacing)return 0;
    }
    for(int type=5;type<=8;++type) {
        if(count_on_page(edit,type)>(old_type==type?1:0))continue;
        struct cell cell;
        if(nearest(grid,type,(struct cell){x,y},&cell,replacing) && cell.x>=0 && cell.x<=4 && cell.y==0) {
            *out=(struct layout){.type=type,.x=cell.x,.y=cell.y,.id=65535};return 1;
        }
    }
    return 0;
}
static void item_data(void *list,void *item,int row) {
    original_data(list,item,row);
    void *button=find_sub(item,24002);if(!button)return;
    struct layout wanted;
    set_text(button,mine(row)?(candidate(&wanted)?"3D Vehicle":"3D Vehicle — no free 2×2 slot"):"");
}
static void item_click(void *list,int row,int child) {
    if(!mine(row)){original_click(list,row,child);return;}
    struct layout wanted;if(!candidate(&wanted))return;
    /* GaugePower::setScale reads its third source before initialize(). Supply
       the native optional data slots even though our refresh getter is inert. */
    void *edit=*caller;unsigned short sources[3]={VEHICLE_SOURCE,0,35};
    wanted.begin=sources;wanted.end=wanted.capacity=sources+3;
    unsigned short replacing=short_at(intent+36);
    /* Keep a copy for rollback if native activation rejects the new widget. */
    struct layout previous={0};unsigned short previous_sources[4];int have_previous=0;
    void *grid=active_grid(book());void **v=widgets(grid);
    for(void **i=v[0];i!=(void **)v[1];++i)if(short_at((unsigned char *)*i+48)==replacing) {
        const unsigned char *p=*i;memcpy(&previous,p+36,12);previous.id=replacing;
        const unsigned short *begin=pointer(p+52),*end=pointer(p+52+sizeof(void *));
        if((uintptr)end<(uintptr)begin || (uintptr)end-(uintptr)begin>sizeof(previous_sources) || !begin)return;
        usize n=(uintptr)end-(uintptr)begin;if(!n || n%2)return;
        memcpy(previous_sources,begin,n);previous.begin=previous_sources;previous.end=previous.capacity=previous_sources+n/2;have_previous=1;break;
    }
    if(replacing!=65535 && !have_previous)return;
    if(have_previous)remove_widget(edit,replacing);
    add_widget(edit,&wanted);
    v=widgets(grid);int found=0;
    for(void **i=v[0];i!=(void **)v[1];++i)if(registry_widget(*i))found=1;
    if(!found) {
        if(have_previous){previous.id=65535;add_widget(edit,&previous);}
        api->log(api,"vehicle-widget: native placement rejected; previous widget restored");return;
    }
    close_editor(edit,1);
}
static void resume_picker(void *self) {
    /* Re-editing a car opens the picker, not the native gauge's data editor. */
    unsigned short id=short_at(intent+36),source=short_at(intent+38),none=65535;
    if(source==VEHICLE_SOURCE)memcpy(intent+36,&none,2);
    picker_resume(self);
    if(source==VEHICLE_SOURCE)memcpy(intent+36,&id,2);
}
static int write_layout(const char *path,const void *json) {
    if(strcmp(path,VEHICLE_LAYOUT_RAM))return json_write(path,json);
    if(__sync_lock_test_and_set(&persistence_busy,1))return 0;
    int ok=json_write(path,json);
    if(ok && layout_save()) {api->log(api,"vehicle-widget: layout could not be saved to dashborne.conf");ok=0;}
    __sync_lock_release(&persistence_busy);return ok;
}
static int edit(void *at,const void *before,const void *after,unsigned int size) {
    if(edit_count>=20)return -1;
    return memory_prepare(&edits[edit_count++],at,before,after,size);
}
static int word(unsigned int offset,uintptr before,uintptr after) {
    return edit(base+offset,&before,&after,4);
}
int registry_bind(const struct dashborne_host *h) {
    if(api)return -1;api=h;
#define BIND(v,lib,s) do {*(void **)(&(v))=h->resolve(lib,s);if(!(v))return -1;}while(0)
#define APP(v,s) BIND(v,h->official_library,s)
#define UI(v,s) BIND(v,h->ui_runtime,s)
    void *symbol=h->resolve(h->official_library,"_ZN7WdgList7getItemE9WdgType_tt");
    struct image_info image;
    if(!symbol || !dladdr(symbol,&image) || (uintptr)symbol!=(uintptr)image.base+0xda1c8)return -1;
    base=image.base;
    APP(stock_special,"_ZN7WdgList7specialE");
    APP(active_grid,"_ZNK10WidgetBook13getActiveGridEv");APP(book,"_ZN10WidgetBook8instanceEv");
    APP(widgets,"_ZNK11DisplayGrid16getActiveWidgetsEv");APP(count_on_page,"_ZN10WidgetEdit11countOnPageE12WdgOutlook_t");
    APP(nearest,"_ZN11DisplayGrid15findNearestFreeE12WdgOutlook_t12WdgGridPos_tRS1_t");
    APP(add_widget,"_ZN10WidgetEdit9addWidgetER9WdgLayout");APP(remove_widget,"_ZN10WidgetEdit12removeWidgetEt");
    APP(close_editor,"_ZN10WidgetEdit11closeEditorEb");
    APP(picker_resume,"_ZN18widgetEditActivity8onResumeEv");
    APP(loader,"_ZN9WdgLoader11getInstanceEv");APP(loader_load,"_ZN9WdgLoader4loadEv");
    UI(json_write,"_ZN10JsonHelper13writeJsonFileEPKcRKN4Json5ValueE");
    UI(find_sub,"_ZNK10ZKListView10ZKListItem15findSubItemByIDEi");UI(set_text,"_ZN10ZKTextView7setTextEPKc");
#undef APP
#undef UI
#undef BIND
    if(sizeof(void *)!=4 || stock_special!=base+0x34e2c4)return -1;
    void *refresh=h->resolve(h->official_library,"_Z13getStat_VideoR11IWidgetBasei");
    if(!refresh)return -1;
    prepare_catalog(stock_special,refresh);
    int n=snprintf(thumbnail,sizeof(thumbnail),"%s/assets/motion/camera-00.png",h->extension_directory);
    if(n<0 || (usize)n>=sizeof(thumbnail))return -1;
    memcpy(options,base+0x3371a8,64);
    options[4]=(struct option){480,288,VEHICLE_SOURCE,thumbnail};
    tab=(int *)(base+0x3593c0);category=(int *)(base+0x35ee00);intent=base+0x35f070;caller=(void **)(base+0x35efc8);
    original_click=(void (*)(void *,int,int))(base+0x128170);
    original_data=(void (*)(void *,void *,int))(base+0x11e6e8);
    /* RTLD_LAZY may leave this import pointing at the image's PLT resolver. */
    void *write_slot=pointer(base+0x358378);
    if(write_slot!=(void *)json_write && write_slot!=base+0x97b78)return -1;
    if(word(0x358c44,(uintptr)stock_special,(uintptr)catalog) ||
       word(0xda218,0xe3510010u,0xe3510011u) ||
       word(0x336e80,4,5) || word(0x336e84,(uintptr)(base+0x3371a8),(uintptr)options) ||
       word(0x3597f4,(uintptr)(base+0x2a0028),(uintptr)category_ids) || word(0x3597f8,4,5) ||
       word(0x336dd8,(uintptr)original_data,(uintptr)item_data) || word(0x336ddc,(uintptr)original_click,(uintptr)item_click) ||
       word(0x358378,(uintptr)write_slot,(uintptr)write_layout) ||
       word(0x3507bc,(uintptr)picker_resume,(uintptr)resume_picker))return -1;
    /* Every reference to the layout pathname, including reset/check paths.
       Change aligned relative-address literals, never a live string in pieces. */
    const unsigned int paths[][2]={{0xb9b78,0xb9a40},{0xba20c,0xba05c},
        {0xdd440,0xdd300},{0xdd444,0xdd360},{0xdf054,0xdeafc},
        {0xdf058,0xdeb80},{0xdf080,0xdecb0}};
    path_edits=edit_count;
    for(unsigned int i=0;i<7;++i)if(word(paths[i][0],0x28dda0-paths[i][1],
        (uintptr)VEHICLE_LAYOUT_RAM-(uintptr)(base+paths[i][1])))return -1;
    if(layout_prepare())return -1;
    for(unsigned int i=0;i<edit_count;++i)if(memory_apply(&edits[i]))return -1;
    registered=1;
    /* InitSystem may have loaded the stock layout before extension activation. */
    if(!loader_load(loader()))return -1;
    h->log(h,"vehicle-widget: stock picker entry registered; layout stored in dashborne.conf");return 0;
}
int registry_cleanup(void) {
    /* Called after all dashboard controls have retired. No custom callback is
       left in a native widget: the source uses the stock no-op refresh getter. */
    if(registered) {
        /* Reload the flash layout while SAVES STILL TARGET RAM. Only restore
           the write path after no custom source remains in the loader. */
        if(memory_restore(&edits[path_edits+4]) || memory_restore(&edits[path_edits+5]) ||
           !loader_load(loader()))return -1;
        registered=0;
    }
    for(unsigned int i=edit_count;i>0;--i)if(memory_restore(&edits[i-1]))return -1;
    registered=0;edit_count=0;api=0;base=0;layout_cleanup();return 0;
}
