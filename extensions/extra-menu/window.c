#include "window.h"
extern void *calloc(__SIZE_TYPE__,__SIZE_TYPE__);
extern void free(void *);
#define ACTIVITY "dashborneExtension_extra_menu"
#define LIST 809200
#define OPEN 299201
#define BACK 299202
static const struct dashborne_host *api;
static void *table[17],*live,*list_view;
static int pending,closing,selected=-1;
static unsigned int count;
static struct dashborne_extra_entry entries[64];
static void (*ctor)(void *),(*dtor)(void *),(*base_created)(void *);
static void *(*find)(void *,int),*(*context)(void);
static void (*text)(void *,const char *),(*visible)(void *,int),(*mark)(void *,int);
static int (*id_of)(void *);
static void (*set_adapter)(void *,void *),(*set_listener)(void *,void *),(*refresh)(void *);
static void (*open_activity)(void *,const char *,void *),(*close_activity)(void *,const char *);
static const char *layout(void *s) {(void)s;return "../../../mnt/extsd/Dashborne/extensions/extra-menu/assets/menu.ftu";}
static void label(int id,const char *value) {void *p=find(live,id);if(p)text(p,value);}
static void show(int id,int value) {void *p=find(live,id);if(p)visible(p,value);}
static void render(void) {
    if(!live)return;
    int valid=selected>=0 && (unsigned int)selected<count;
    label(599202,valid?entries[selected].title:"Extensions");
    label(599203,valid?entries[selected].description:"");
    show(599204,!count);show(OPEN,valid);show(LIST,count!=0);
}
static void snapshot(void) {
    unsigned int token=selected>=0 && (unsigned int)selected<count?entries[selected].token:0;
    count=api->extra_count(api);if(count>64)count=64;
    selected=-1;
    for(unsigned int i=0;i<count;++i) {
        if(api->extra_entry(api,i,&entries[i])) {count=i;break;}
        if(entries[i].token==token)selected=(int)i;
    }
    if(selected<0 && count)selected=0;
    if(list_view)refresh(list_view);
    render();
}
static void noop(void *s) {(void)s;}
static int row_count(void *s,void *l) {(void)s;(void)l;return (int)count;}
static void row_data(void *s,void *l,void *item,int index) {
    (void)s;(void)l;
    if(index<0 || (unsigned int)index>=count)return;
    text(item,entries[index].title);mark(item,index==selected);
}
static void row_click(void *s,void *l,int index,int sub) {
    (void)s;(void)sub;
    if(!live || closing || index<0 || (unsigned int)index>=count)return;
    selected=index;refresh(l);render();
}
static void *adapter_table[]={0,0,(void *)noop,(void *)noop,(void *)row_count,(void *)row_data};
static void *listener_table[]={0,0,(void *)noop,(void *)noop,(void *)row_click};
static void **adapter=adapter_table+2,**listener=listener_table+2;
static void back(void *s) {
    (void)s;if(closing)return;
    closing=1;close_activity(context(),ACTIVITY);
}
static int hardware_back(void *s) {back(s);return 1;}
static void click(void *s,void *c) {
    if(closing)return;
    int id=id_of(c);
    if(id==BACK)back(s);
    else if(id==OPEN && selected>=0 && (unsigned int)selected<count) {
        if(api->open_extra(api,entries[selected].token))snapshot();
    }
}
static void created(void *s) {
    base_created(s);pending=0;closing=0;
    list_view=find(s,LIST);
    if(list_view) {set_adapter(list_view,&adapter);set_listener(list_view,&listener);}
    snapshot();
}
static void resumed(void *s) {(void)s;closing=0;snapshot();}
static int timer(void *s,int id) {(void)s;(void)id;return 1;}
static int timer_secondary(void *s,int id) {return timer((char *)s-sizeof(void *),id);}
static void destroy_only(void *s) {
    if(list_view) {set_listener(list_view,0);set_adapter(list_view,0);}
    list_view=0;if(live==s)live=0;pending=0;dtor(s);
}
static void destroy(void *s) {destroy_only(s);free(s);}
static void destroy_only_secondary(void *s) {destroy_only((char *)s-sizeof(void *));}
static void destroy_secondary(void *s) {destroy((char *)s-sizeof(void *));}
static void *create(void) {
    if(live)return 0;
    void **p=calloc(6,sizeof(void *));if(!p) {pending=0;return 0;}
    ctor(p);p[0]=table+2;p[1]=table+14;live=p;return p;
}
void extra_window_open(void) {
    if(!api || pending || live)return;
    pending=1;open_activity(context(),ACTIVITY,0);
}
int extra_window_bind(const struct dashborne_host *host) {
    api=host;
#define BIND(v,s) do {*(void **)(&(v))=api->resolve(api->ui_runtime,s);if(!(v))return -1;}while(0)
    BIND(ctor,"_ZN8ActivityC1Ev");BIND(dtor,"_ZN8ActivityD1Ev");
    BIND(base_created,"_ZN7BaseApp8onCreateEv");BIND(find,"_ZN7BaseApp15findControlByIDEi");
    BIND(text,"_ZN10ZKTextView7setTextEPKc");BIND(visible,"_ZN6ZKBase10setVisibleEb");
    BIND(mark,"_ZN6ZKBase11setSelectedEb");BIND(id_of,"_ZNK6ZKBase5getIDEv");
    BIND(set_adapter,"_ZN10ZKListView14setListAdapterEPNS_14AbsListAdapterE");
    BIND(set_listener,"_ZN10ZKListView20setItemClickListenerEPNS_18IItemClickListenerE");
    BIND(refresh,"_ZN10ZKListView15refreshListViewEv");
    BIND(context,"_ZN13EasyUIContext11getInstanceEv");
    BIND(open_activity,"_ZN13EasyUIContext12openActivityEPKcP6Intent");
    BIND(close_activity,"_ZN13EasyUIContext13closeActivityEPKc");
#undef BIND
    void **original=api->resolve(api->ui_runtime,"_ZTV8Activity");
    if(!original || original[6]!=(void *)base_created ||
       (__PTRDIFF_TYPE__)original[12]!=-(__PTRDIFF_TYPE__)sizeof(void *))return -1;
    for(unsigned int i=0;i<17;++i)table[i]=original[i];
    table[2]=(void *)destroy_only;table[3]=(void *)destroy;table[4]=(void *)click;
    table[5]=(void *)layout;table[6]=(void *)created;table[7]=(void *)timer;
    table[9]=(void *)resumed;table[10]=(void *)noop;table[11]=(void *)hardware_back;
    table[14]=(void *)destroy_only_secondary;table[15]=(void *)destroy_secondary;table[16]=(void *)timer_secondary;
    return api->register_activity(api,ACTIVITY,create);
}
int extra_window_cleanup(void) {
    if(live || pending)return -1;
    api=0;count=0;selected=-1;return 0;
}
