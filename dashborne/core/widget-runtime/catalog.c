#include "../services.h"
#include "runtime.h"
#include "../../config.h"
extern int strcmp(const char *,const char *),snprintf(char *,usize,const char *,...);
extern usize strlen(const char *);
const struct dashborne_host *runtime_host;
struct widget_provider providers[PROVIDERS];
const struct widget_geometry widget_sizes[DBW_SIZE_COUNT]={
    {1,1,0,250,224},{2,1,2,510,224},{1,2,4,250,458},{2,2,5,510,458},
    {3,2,13,770,458},{4,2,14,1030,458},{5,2,15,1290,458},{6,2,16,1550,458}
};
const struct widget_provider *provider_for(unsigned int source) {
    for(int i=0;i<PROVIDERS;++i)if(providers[i].host && providers[i].source==source)return &providers[i];
    return 0;
}
static int valid_id(const char *s) {
    if(!s || !*s || strlen(s)>63)return 0;
    for(;*s;++s)if(!((*s>='a'&&*s<='z')||(*s>='0'&&*s<='9')||*s=='-'||*s=='_'||*s=='.'))return 0;
    return 1;
}
static unsigned int identity(const char *id) {
    /* Preserve the original car's saved source; other IDs never move when
       registration order changes or an extension is temporarily disabled. */
    if(!strcmp(id,"vehicle-widget"))return FIRST_SOURCE;
    if(!config_load())return 0;
    char key[64],value[1024];unsigned int empty=0,result=0;
    for(unsigned int n=FIRST_SOURCE+1;n<=LAST_SOURCE;++n) {
        snprintf(key,sizeof(key),"widgets.source.%u",n);
        if(config_get(key,value,sizeof(value))) {if(!strcmp(value,id)){result=n;break;}}
        else if(!empty)empty=n;
    }
    if(!result && empty) {
        snprintf(key,sizeof(key),"widgets.source.%u",empty);
        if(config_set(key,id)&&config_commit())result=empty;
    }
    config_clear();return result;
}
static int register_widget(const struct dashborne_host *h,const struct dashborne_widget *w) {
    static const unsigned int categories[]={4,12,9};
    if(!runtime_host || runtime_live() || !h || !w || w->abi_version!=1 || w->struct_size<DASHBORNE_WIDGET_V1_SIZE ||
       !valid_id(w->id) || !w->title || !*w->title || strlen(w->title)>63 || !w->description ||
       strlen(w->description)>127 || !w->sizes || (w->sizes&~DBW_ALL_SIZES) || w->tab>2 || w->category>=categories[w->tab])return -1;
    int slot=-1;
    for(int i=0;i<PROVIDERS;++i) {
        if(providers[i].host && (providers[i].host==h || !strcmp(providers[i].widget->id,w->id)))return -1;
        if(!providers[i].host && slot<0)slot=i;
    }
    if(slot<0 || runtime_bind())return -1;
    unsigned int source=identity(w->id);if(!source || registry_publish(source,w))return -1;
    providers[slot]=(struct widget_provider){h,w,source};return 0;
}
static int unregister_widget(const struct dashborne_host *h) {
    if(runtime_live())return -1;
    for(int i=0;i<PROVIDERS;++i)if(providers[i].host==h) {
        registry_publish(providers[i].source,0);providers[i]=(struct widget_provider){0};
    }
    for(int i=0;i<PROVIDERS;++i)if(providers[i].host)return 0;
    /* Binding happens with the first provider, after earlier extensions have
       installed their hooks. Unwind with the last provider so those extensions
       can then restore their callbacks. */
    return runtime_unbind();
}
DASHBORNE_EXTENSION_EXPORT const struct dashborne_widgets *dashborne_widgets_v1(void) {
    static const struct dashborne_widgets api={1,sizeof(api),register_widget,unregister_widget,views_text,views_invalidate};return &api;
}
int core_widgets_start(const struct dashborne_host *h){if(runtime_host)return -1;runtime_host=h;return 0;}
int core_widgets_stop(void) {
    for(int i=0;i<PROVIDERS;++i)if(providers[i].host)return -1;
    if(runtime_unbind())return -1;
    runtime_host=0;return 0;
}
