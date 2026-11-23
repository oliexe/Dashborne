#include "window.h"
typedef __UINTPTR_TYPE__ uintptr;
extern int mprotect(void *,__SIZE_TYPE__,int);
extern void *fopen(const char *,const char *);
extern char *fgets(char *,int,void *);
extern int fclose(void *),sscanf(const char *,const char *,...);
static const struct dashborne_host *api;
static void **table;
static int installed, own_index=-1, page_count;
static struct {void *p;int prot;} pages[2];
static int (*id_of)(void *),(*original_count)(void *,void *);
static void (*text)(void *,const char *),(*selected)(void *,int);
static void (*original_data)(void *,void *,void *,int);
static void (*original_click)(void *,void *,int,int);
static int topics(void *p) {return p && id_of(p)==80001;}
static int count(void *s,void *l) {
    int n=original_count(s,l);
    if(topics(l) && n>=0 && n<64) {own_index=n;return n+1;}
    return n;
}
static void data(void *s,void *l,void *i,int n) {
    if(topics(l) && n==own_index) {text(i,"Screen Mirroring");selected(i,0);return;}
    original_data(s,l,i,n);
}
static void click(void *s,void *l,int n,int sub) {
    if(topics(l) && n==own_index) {window_open();return;}
    original_click(s,l,n,sub);
}
static int count_secondary(void *s,void *l) {return count((char *)s-32,l);}
static void data_secondary(void *s,void *l,void *i,int n) {data((char *)s-32,l,i,n);}
static void click_secondary(void *s,void *l,int n,int sub) {click((char *)s-28,l,n,sub);}
static struct {int index;const char *symbol;void *replacement,*saved;} hooks[]={
 {13,"_ZNK19awlsettingsActivity16getListItemCountEPK10ZKListView",(void *)count,0},
 {14,"_ZN19awlsettingsActivity18obtainListItemDataEP10ZKListViewPNS0_10ZKListItemEi",(void *)data,0},
 {15,"_ZN19awlsettingsActivity11onItemClickEP10ZKListViewii",(void *)click,0},
 {37,"_ZThn28_N19awlsettingsActivity11onItemClickEP10ZKListViewii",(void *)click_secondary,0},
 {42,"_ZThn32_NK19awlsettingsActivity16getListItemCountEPK10ZKListView",(void *)count_secondary,0},
 {43,"_ZThn32_N19awlsettingsActivity18obtainListItemDataEP10ZKListViewPNS0_10ZKListItemEi",(void *)data_secondary,0}
};
#define N (sizeof(hooks)/sizeof(hooks[0]))
static int protect(int write) {
    int failed=0;
    for(int i=0;i<page_count;++i)if(mprotect(pages[i].p,4096,pages[i].prot|(write?2:0)))failed=1;
    return failed?-1:0;
}
static int map(void) {
    uintptr start=(uintptr)table&~(uintptr)4095,last=(uintptr)(table+43)&~(uintptr)4095;
    page_count=start==last?1:2;
    for(int i=0;i<page_count;++i) {
        uintptr addr=start+4096u*(unsigned int)i;
        void *f=fopen("/proc/self/maps","r");if(!f)return -1;
        char line[512],perms[8];unsigned long a,b;int found=0;
        while(fgets(line,sizeof(line),f))if(sscanf(line,"%lx-%lx %4s",&a,&b,perms)==3 && addr>=a && addr+4096<=b) {
            if(perms[0]=='r' && perms[3]=='p') {
                pages[i].p=(void *)addr;pages[i].prot=1|(perms[1]=='w'?2:0)|(perms[2]=='x'?4:0);found=1;
            }
            break;
        }
        fclose(f);if(!found)return -1;
    }
    return 0;
}
int settings_bind(const struct dashborne_host *host) {
    if(api)return -1;
    api=host;
#define BIND(v,s) do {*(void **)(&(v))=api->resolve(api->ui_runtime,s);if(!(v))return -1;}while(0)
    BIND(id_of,"_ZNK6ZKBase5getIDEv");BIND(text,"_ZN10ZKTextView7setTextEPKc");
    BIND(selected,"_ZN6ZKBase11setSelectedEb");
#undef BIND
    table=api->resolve(api->official_library,"_ZTV19awlsettingsActivity");if(!table)return -1;
    for(unsigned int i=0;i<N;++i) {
        void *stock=api->resolve(api->official_library,hooks[i].symbol);
        if(!stock || (table[hooks[i].index]!=stock && !api->owns_callback(api,table[hooks[i].index])))return -1;
        hooks[i].saved=table[hooks[i].index];
    }
    *(void **)(&original_count)=hooks[0].saved;*(void **)(&original_data)=hooks[1].saved;
    *(void **)(&original_click)=hooks[2].saved;
    if(map())return -1;
    if(protect(1)) {protect(0);return -1;}
    for(unsigned int i=0;i<N;++i)table[hooks[i].index]=hooks[i].replacement;
    installed=1;
    return protect(0);
}
int settings_cleanup(void) {
    if(installed) {
        for(unsigned int i=0;i<N;++i)
            if(table[hooks[i].index]!=hooks[i].replacement && table[hooks[i].index]!=hooks[i].saved)return -1;
        if(protect(1)) {protect(0);return -1;}
        for(unsigned int i=0;i<N;++i)table[hooks[i].index]=hooks[i].saved;
        if(protect(0))return -1;
    }
    installed=0;api=0;table=0;own_index=-1;return 0;
}
