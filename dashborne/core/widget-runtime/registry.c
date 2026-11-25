#include "stock_compat.h"
#include "runtime.h"
#include "memory.h"
#include "storage.h"
typedef __SIZE_TYPE__ usize;
typedef __UINTPTR_TYPE__ uintptr;
extern int snprintf(char *,usize,const char *,...),strcmp(const char *,const char *);
extern void *memcpy(void *,const void *,usize);
extern int access(const char *,int);
struct image_info {const char *name;void *base;const char *symbol;void *address;};
extern int dladdr(const void *,struct image_info *);
static const struct dashborne_host *api;
static unsigned char *base;
static struct memory_edit edits[32];
static unsigned int edit_count;
static void *stock_special;
/* Descriptors are copied in RAM. Reserved custom IDs remain inert when their
   provider is disabled, so old placements can still be removed in the editor. */
static uintptr catalog[256][18];
static int *category;
static unsigned char *intent;
static void **caller;
static void (*original_click[3])(void *,int,int),(*original_data[3])(void *,void *,int);
static int (*original_count[3])(void *);
static void (*set_position)(void *,const void *),(*set_picture)(void *,int,const char *);
static void (*set_icon_position)(void *,const void *);
static int (*get_id)(const void *);
struct rect {int x,y,w,h;};
static void (*picker_resume)(void *);
static int (*json_write)(const char *,const void *);
static void *(*find_sub)(const void *,int);
static void (*set_text)(void *,const char *);
static void *(*active_grid)(void *),*(*book)(void),*(*widgets)(void *),*(*loader)(void);
static int (*loader_load)(void *);
static int (*nearest)(void *,int,struct cell,struct cell *,unsigned short);
static void (*add_widget)(void *,struct layout *),(*remove_widget)(void *,unsigned short),(*close_editor)(void *,int);
static unsigned int registered;
static unsigned int path_edits;
static int persistence_busy;
static void *pointer(const void *p){void *v;memcpy(&v,p,sizeof(v));return v;}
static unsigned short short_at(const void *p){unsigned short v;memcpy(&v,p,sizeof(v));return v;}
unsigned int registry_source(const void *w) {
    if(!w)return 0;
    const unsigned char *p=w;
    const unsigned short *begin=pointer(p+52),*end=pointer(p+52+sizeof(void *));
    uintptr a=(uintptr)begin,b=(uintptr)end;
    if(!a || b<a || b-a<2 || b-a>8 || ((a|b)&1))return 0;
    unsigned int source=short_at(begin);
    return source>=FIRST_SOURCE && source<=LAST_SOURCE?source:0;
}
int registry_publish(unsigned int source,const struct dashborne_widget *w) {
    if(source<FIRST_SOURCE || source>LAST_SOURCE)return -1;
    uintptr *d=catalog[source&255];
    d[0]=d[1]=(uintptr)(w?w->title:"Unavailable widget");
    d[17]=(uintptr)(w?w->description:"Enable its extension to restore this widget.");return 0;
}
static void prepare_catalog(const uintptr *stock,void *refresh) {
    for(unsigned int i=0;i<17*18;++i)((uintptr *)catalog)[i]=stock[i];
    for(unsigned int n=17;n<256;++n) {
        uintptr *d=catalog[n];
        for(unsigned int i=0;i<18;++i)d[i]=stock[18+i];
        d[2]=d[3]=(uintptr)"";d[15]=(uintptr)refresh;d[16]=0;
        registry_publish(0x200+n,0);
    }
}
static int list_tab(void *list) {
    int id=get_id(list);return id==80002?0:id==80003?1:id==80004?2:-1;
}
static int stock_count(int t) {
    if(t<0 || t>2)return 0;
    const unsigned int offsets[]={0x3598e0,0x359850,0x3597e4},counts[]={4,12,9};
    int c=category[t];if(c<0 || (unsigned int)c>=counts[t])return 0;
    unsigned int n;
    /* Special rows are presentation templates, not the category's source IDs:
       Trip has six sources but one row; Navigation has one source but two rows. */
    memcpy(&n,base+(t==2?0x336e78+c*8:offsets[t]+c*12+8),4);
    return n<=64?(int)n:0;
}
static const struct widget_provider *row_provider(int t,int row,int *size) {
    row-=stock_count(t);if(row<0 || t<0 || t>2)return 0;
    for(int i=0;i<PROVIDERS;++i) {
        const struct widget_provider *p=&providers[i];
        if(!p->host || p->widget->tab!=(unsigned int)t || p->widget->category!=(unsigned int)category[t])continue;
        for(int s=0;s<DBW_SIZE_COUNT;++s)if(p->widget->sizes&(1u<<s)) {
            if(!row--){*size=s;return p;}
        }
    }
    return 0;
}
static int item_count(void *list) {
    int t=list_tab(list);if(t<0)return 0;
    int count=original_count[t](list),extra=0;
    for(int i=0;i<PROVIDERS;++i)if(providers[i].host && providers[i].widget->tab==(unsigned int)t &&
       providers[i].widget->category==(unsigned int)category[t])
        for(int s=0;s<DBW_SIZE_COUNT;++s)extra+=(providers[i].widget->sizes>>s)&1;
    /* Special's single-stock-item category otherwise narrows the list. */
    if(t==2 && extra)set_position(list,&(struct rect){520,11,1030,469});
    return count+extra;
}
#ifdef DASHBORNE_WIDGET_DEVICE_TEST
/* Run on the UI thread. Native count callbacks use the selected tab/category
   globals; Special also positions its list. Borrow a live control for that
   position call, then restore every field before returning to the event loop. */
int registry_device_probe(void *control) {
    int *tab=(int *)(base+0x3593c0),saved_tab=*tab,saved_category[3];
    void *saved_list=pointer(base+0x35ee10);
    memcpy(saved_category,category,sizeof(saved_category));
    memcpy(base+0x35ee10,&control,sizeof(control));
    const int counts[]={4,12,9};int ok=1,checked=0;
    struct widget_provider *provider=0;
    for(int i=0;i<PROVIDERS;++i)if(providers[i].host && providers[i].source!=FIRST_SOURCE){provider=&providers[i];break;}
    const struct dashborne_widget *saved_widget=provider?provider->widget:0;
    struct dashborne_widget definition={0};
    if(saved_widget){memcpy(&definition,saved_widget,saved_widget->struct_size<sizeof(definition)?saved_widget->struct_size:sizeof(definition));definition.sizes=DBW_ALL_SIZES;provider->widget=&definition;}
    else ok=0;
    for(int t=0;ok && t<3;++t)for(int c=0;ok && c<counts[t];++c) {
        *tab=t;category[t]=c;definition.tab=(unsigned int)t;definition.category=(unsigned int)c;
        int native=original_count[t](control),expected=stock_count(t);
        if(native!=expected){ok=0;break;}
        /* Other registered providers may precede this one in the same list. */
        for(int s=0;s<DBW_SIZE_COUNT;++s) {
            int found=0;
            for(int row=expected;row<expected+PROVIDERS*DBW_SIZE_COUNT;++row) {
                int size=-1;const struct widget_provider *p=row_provider(t,row,&size);
                if(!p)break;
                if(p==provider && size==s){found=1;break;}
            }
            if(!found)ok=0;
        }
        ++checked;
    }
    if(provider)provider->widget=saved_widget;
    *tab=saved_tab;memcpy(category,saved_category,sizeof(saved_category));
    memcpy(base+0x35ee10,&saved_list,sizeof(saved_list));
    char message[128];snprintf(message,sizeof(message),"widget-check: %s native picker counts and eight sizes in %d categories",ok?"PASS":"FAIL",checked);
    api->log(api,message);return ok;
}
#endif
static int candidate(struct layout *out,int size) {
    if(size<0 || size>=DBW_SIZE_COUNT)return 0;
    void *edit=*caller;if(!edit || !intent[0] || intent[0]>25)return 0;
    void *grid=active_grid(book());if(!grid || pointer((unsigned char *)edit+48)!=grid)return 0;
    int x,y;memcpy(&x,intent+4,4);memcpy(&y,intent+8,4);
    if(x<0 || x>5 || y<0 || y>1)return 0;
    struct cell cell;const struct widget_geometry *g=&widget_sizes[size];
    if(!nearest(grid,g->type,(struct cell){x,y},&cell,short_at(intent+36)) ||
       cell.x<0 || cell.y<0 || cell.x+g->columns>6 || cell.y+g->rows>2)return 0;
    *out=(struct layout){.type=g->type,.x=cell.x,.y=cell.y,.id=65535};return 1;
}
static const char *preview_picture(const struct dashborne_widget *w,int size) {
    static const char *fallback[]={
        "../../../mnt/extsd/Dashborne/assets/widget-runtime/preview-1x1.png",
        "../../../mnt/extsd/Dashborne/assets/widget-runtime/preview-2x1.png",
        "../../../mnt/extsd/Dashborne/assets/widget-runtime/preview-1x2.png",
        "../../../mnt/extsd/Dashborne/assets/widget-runtime/preview-2x2.png",
        "../../../mnt/extsd/Dashborne/assets/widget-runtime/preview-3x2.png",
        "../../../mnt/extsd/Dashborne/assets/widget-runtime/preview-4x2.png",
        "../../../mnt/extsd/Dashborne/assets/widget-runtime/preview-5x2.png",
        "../../../mnt/extsd/Dashborne/assets/widget-runtime/preview-6x2.png"};
    if(w->struct_size>=__builtin_offsetof(struct dashborne_widget,preview_images)+(size+1)*sizeof(char *)) {
        const char *path=w->preview_images[size];
        if(path && *path && !access(path,4))return path;
    }
    return fallback[size];
}
static struct rect preview_bounds(int tab,int size) {
    const struct widget_geometry *g=&widget_sizes[size];
    int width=tab==2?480:218,height=tab==2?312:132;
    int w=width,h=w*g->height/g->width;
    if(h>height){h=height;w=h*g->width/g->height;}
    return (struct rect){(tab==2?15:16)+(width-w)/2,8+(height-h)/2,w,h};
}
static void item_data(void *list,void *item,int row) {
    int t=list_tab(list),size=0;if(t<0 || row<0)return;
    const struct widget_provider *p=row_provider(t,row,&size);
    /* Populate native styling and scroll arrows using a valid stock row. */
    void *label=find_sub(item,t==0?24001:t==1?24007:24002);
    /* List items are recycled across stock/custom rows and categories. Remove
       our picture before stock repopulates the item, including on other tabs. */
    if(label)set_picture(label,0,"");
    if(t==2 && label)set_text(label,"");
    original_data[t](list,item,p?0:row);
    if(!p)return;
    char title[160];struct layout wanted;
    const struct widget_geometry *g=&widget_sizes[size];
    snprintf(title,sizeof(title),"%s\n%d × %d%s",p->widget->title,g->columns,g->rows,candidate(&wanted,size)?"":" — no space");
    if(label)set_text(label,title);
    if(t==0) {
        void *value=find_sub(item,24005),*unit=find_sub(item,24006);
        if(value)set_text(value,"");if(unit)set_text(unit,"");
    } else if(t==1) {void *icon=find_sub(item,24017);if(icon)set_text(icon,"");}
    if(label) {
        struct rect bounds=preview_bounds(t,size);
        set_icon_position(label,&bounds);
        set_picture(label,0,preview_picture(p->widget,size));
    }
}
static void item_click(void *list,int row,int child) {
    int t=list_tab(list),size=0;if(t<0 || row<0)return;
    const struct widget_provider *p=row_provider(t,row,&size);
    if(!p) {if(row<stock_count(t))original_click[t](list,row,child);return;}
    struct layout wanted;if(!candidate(&wanted,size))return;
    void *edit=*caller;unsigned short sources[3]={(unsigned short)p->source,0,35};
    wanted.begin=sources;wanted.end=wanted.capacity=sources+3;
    unsigned short replacing=short_at(intent+36);
    struct layout previous={0};unsigned short previous_sources[4];int have_previous=0;
    void *grid=active_grid(book());void **v=widgets(grid);
    for(void **i=v[0];i!=(void **)v[1];++i)if(short_at((unsigned char *)*i+48)==replacing) {
        const unsigned char *q=*i;memcpy(&previous,q+36,12);previous.id=replacing;
        const unsigned short *begin=pointer(q+52),*end=pointer(q+52+sizeof(void *));
        if(!begin || (uintptr)end<(uintptr)begin || (uintptr)end-(uintptr)begin>sizeof(previous_sources))return;
        usize n=(uintptr)end-(uintptr)begin;if(!n || n%2)return;
        memcpy(previous_sources,begin,n);previous.begin=previous_sources;previous.end=previous.capacity=previous_sources+n/2;have_previous=1;break;
    }
    if(replacing!=65535 && !have_previous)return;
    if(have_previous)remove_widget(edit,replacing);
    add_widget(edit,&wanted);
    v=widgets(grid);int found=0;
    for(void **i=v[0];i!=(void **)v[1];++i)if(registry_source(*i)==p->source && short_at((unsigned char *)*i+48)==wanted.id)found=1;
    if(!found) {
        if(have_previous){previous.id=65535;add_widget(edit,&previous);}
        api->log(api,"widgets: placement rejected; previous widget restored");return;
    }
    close_editor(edit,1);
}
static void resume_picker(void *self) {
    /* Re-editing a car opens the picker, not the native gauge's data editor. */
    unsigned short id=short_at(intent+36),source=short_at(intent+38),none=65535;
    if(source>=FIRST_SOURCE && source<=LAST_SOURCE)memcpy(intent+36,&none,2);
    picker_resume(self);
    if(source>=FIRST_SOURCE && source<=LAST_SOURCE)memcpy(intent+36,&id,2);
}
static int write_layout(const char *path,const void *json) {
    if(strcmp(path,WIDGET_LAYOUT_RAM))return json_write(path,json);
    if(__sync_lock_test_and_set(&persistence_busy,1))return 0;
    int ok=json_write(path,json);
    if(ok && layout_save()) {api->log(api,"widgets: layout could not be saved to dashborne.conf");ok=0;}
    __sync_lock_release(&persistence_busy);return ok;
}
static int edit(void *at,const void *before,const void *after,unsigned int size) {
    if(edit_count>=32)return -1;
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
    if(!symbol || !dladdr(symbol,&image) || (uintptr)symbol!=(uintptr)image.base+DB_STOCK_OFFSET(0xda1c8,0xe670c))return -1;
    base=image.base;
    APP(stock_special,"_ZN7WdgList7specialE");
    APP(active_grid,"_ZNK10WidgetBook13getActiveGridEv");APP(book,"_ZN10WidgetBook8instanceEv");
    APP(widgets,"_ZNK11DisplayGrid16getActiveWidgetsEv");
    APP(nearest,"_ZN11DisplayGrid15findNearestFreeE12WdgOutlook_t12WdgGridPos_tRS1_t");
    APP(add_widget,"_ZN10WidgetEdit9addWidgetER9WdgLayout");APP(remove_widget,"_ZN10WidgetEdit12removeWidgetEt");
    APP(close_editor,"_ZN10WidgetEdit11closeEditorEb");
    APP(picker_resume,"_ZN18widgetEditActivity8onResumeEv");
    APP(loader,"_ZN9WdgLoader11getInstanceEv");APP(loader_load,"_ZN9WdgLoader4loadEv");
    UI(set_position,"_ZN6ZKBase11setPositionERK14LayoutPosition");
    UI(get_id,"_ZNK6ZKBase5getIDEv");UI(set_picture,"_ZN8ZKButton18setButtonStatusPicEiPKc");
    UI(set_icon_position,"_ZN8ZKButton15setIconPositionERK14LayoutPosition");
    UI(json_write,"_ZN10JsonHelper13writeJsonFileEPKcRKN4Json5ValueE");
    UI(find_sub,"_ZNK10ZKListView10ZKListItem15findSubItemByIDEi");UI(set_text,"_ZN10ZKTextView7setTextEPKc");
#undef APP
#undef UI
#undef BIND
    if(sizeof(void *)!=4 || stock_special!=base+DB_STOCK_OFFSET(0x34e2c4,0x34e2f0))return -1;
    void *refresh=h->resolve(h->official_library,"_Z13getStat_VideoR11IWidgetBasei");
    if(!refresh)return -1;
    prepare_catalog(stock_special,refresh);
    category=(int *)(base+0x35ee00);intent=base+0x35f070;caller=(void **)(base+0x35efc8);
    const unsigned int tables[]={0x336df0,0x336de0,0x336dd0};
    for(int t=0;t<3;++t) {
        unsigned char *d=base+tables[t];
        original_count[t]=pointer(d+4);original_data[t]=pointer(d+8);original_click[t]=pointer(d+12);
        if(word(tables[t]+4,(uintptr)original_count[t],(uintptr)item_count) ||
           word(tables[t]+8,(uintptr)original_data[t],(uintptr)item_data) ||
           word(tables[t]+12,(uintptr)original_click[t],(uintptr)item_click))return -1;
    }
    /* RTLD_LAZY may leave this import pointing at the image's PLT resolver. */
    void *write_slot=pointer(base+0x358378);
    if(write_slot!=(void *)json_write && write_slot!=base+DB_STOCK_OFFSET(0x97b78,0x97ba8))return -1;
    if(word(0x358c44,(uintptr)stock_special,(uintptr)catalog) ||
       word(DB_STOCK_OFFSET(0xda218,0xe675c),0xe3510010u,0xe35100ffu) ||
       word(0x358378,(uintptr)write_slot,(uintptr)write_layout) ||
       word(0x3507bc,(uintptr)picker_resume,(uintptr)resume_picker))return -1;
    /* Every reference to the layout pathname, including reset/check paths.
       Change aligned relative-address literals, never a live string in pieces. */
    const unsigned int legacy_paths[][2]={{0xb9b78,0xb9a40},{0xba20c,0xba05c},
        {0xdd440,0xdd300},{0xdd444,0xdd360},{0xdf054,0xdeafc},
        {0xdf058,0xdeb80},{0xdf080,0xdecb0}};
    const unsigned int current_paths[][2]={{0xc65ac,0xc6474},{0xc6c40,0xc6a90},
        {0x1046e4,0x1045a4},{0x1046e8,0x104604},{0xea680,0xea128},
        {0xea684,0xea1ac},{0xea6ac,0xea2dc}};
    const unsigned int (*paths)[2]=db_stock_version()==6114?current_paths:legacy_paths;
    path_edits=edit_count;
    for(unsigned int i=0;i<7;++i)if(word(paths[i][0],DB_STOCK_OFFSET(0x28dda0,0x2919bc)-paths[i][1],
        (uintptr)WIDGET_LAYOUT_RAM-(uintptr)(base+paths[i][1])))return -1;
    if(layout_prepare())return -1;
    for(unsigned int i=0;i<edit_count;++i)if(memory_apply(&edits[i]))return -1;
    registered=1;
    /* InitSystem may have loaded the stock layout before extension activation. */
    if(!loader_load(loader()))return -1;
    h->log(h,"widgets: picker and card layout registered");return 0;
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
