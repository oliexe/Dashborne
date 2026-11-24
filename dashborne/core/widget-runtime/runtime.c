#include "runtime.h"
extern int snprintf(char *,usize,const char *,...);
extern int strcmp(const char *,const char *);
extern usize strlen(const char *);
extern void *memcpy(void *,const void *,usize);
extern int mprotect(void *,usize,int);
extern void *fopen(const char *,const char *);
extern char *fgets(char *,int,void *);
extern int fclose(void *),sscanf(const char *,const char *,...);

#define TIMER 97400
#define TABLE_WORDS 63
struct bitmap {void *words[6];int initialized;};
struct hook {int index;void *before,*after;};
static const struct dashborne_host *host;
static void *live;
static void **activity_table;
static void *hook_page;
static int hook_protection,installed,hook_count;
static struct hook hooks[24];
static void (*stock_create)(void *),(*stock_pause)(void *),(*stock_resume)(void *);
static void (*stock_destroy)(void *),(*stock_delete)(void *),(*stock_click)(void *,void *);
static int (*stock_timer)(void *,int),(*stock_timer_secondary)(void *,int);
static void (*register_timer)(void *,int,int),(*unregister_timer)(void *,int);
struct timestamp {long sec,nsec;};
extern int clock_gettime(int,struct timestamp *);
static unsigned long long microseconds(void) {
    struct timestamp t={0};if(clock_gettime(1,&t))return 0;
    return (unsigned long long)t.sec*1000000+(unsigned long)t.nsec/1000;
}
static void created(void *self) {
    live=self;views_activity(self);views_pause(0);stock_create(self);
    register_timer(self,TIMER,16);host->log(host,"widgets: native page runtime active");
}
static int timer(void *self,int id) {
    if(id!=TIMER)return stock_timer(self,id);
    if(self==live) {
        unsigned long long now=microseconds();views_tick(now);
#ifdef DASHBORNE_WIDGET_DEVICE_TEST
        extern void widget_device_probe(unsigned long long);
        widget_device_probe(now);
#endif
    }
    return 1;
}
static int timer_secondary(void *self,int id) {
    if(id!=TIMER)return stock_timer_secondary(self,id);
    return timer((unsigned char *)self-sizeof(void *),id);
}
static void clicked(void *self,void *control) {
    if(self==live && views_click(control))return;
    stock_click(self,control);
}
static void pause_view(void *self) {
    if(self==live){unregister_timer(self,TIMER);views_pause(1);}stock_pause(self);
}
static void resume_view(void *self) {
    stock_resume(self);if(self==live){views_pause(0);register_timer(self,TIMER,16);}
}
static void retiring(void *self) {
    if(self==live){unregister_timer(self,TIMER);views_pause(1);views_detach();live=0;views_activity(0);}
}
static void destroy(void *self) {retiring(self);stock_destroy(self);}
static void deleting(void *self) {retiring(self);stock_delete(self);}
static void destroy_secondary(void *self) {
    __PTRDIFF_TYPE__ offset=(__PTRDIFF_TYPE__)(*(void ***)self)[-2];destroy((unsigned char *)self+offset);
}
static void delete_secondary(void *self) {
    __PTRDIFF_TYPE__ offset=(__PTRDIFF_TYPE__)(*(void ***)self)[-2];deleting((unsigned char *)self+offset);
}
extern int open(const char *,int,...),read(int,void *,unsigned int),close(int);
extern unsigned long crc32(unsigned long,const unsigned char *,unsigned int);
static int fingerprint(const char *path,unsigned int size,unsigned int expected) {
    int fd=open(path,0);if(fd<0)return 0;
    unsigned char buffer[4096];unsigned int count=0;unsigned long crc=0;int n;
    while((n=read(fd,buffer,sizeof(buffer)))>0) {
        if((unsigned int)n>size-count){close(fd);return 0;}
        count+=(unsigned int)n;crc=crc32(crc,buffer,(unsigned int)n);
    }
    close(fd);return n==0 && count==size && crc==expected;
}
int runtime_live(void){return live!=0;}
static int add_hook(int index,void *expected,void *replacement,int chained) {
    if(!expected || hook_count>=24 ||
       (activity_table[index]!=expected && (!chained || !host->owns_callback || !host->owns_callback(host,activity_table[index]))))return -1;
    hooks[hook_count++]=(struct hook){index,activity_table[index],replacement};return 0;
}
static int map_table(void) {
    uintptr address=(uintptr)activity_table&~(uintptr)4095;
    if((uintptr)(activity_table+TABLE_WORDS)>address+4096)return -1;
    void *file=fopen("/proc/self/maps","r");if(!file)return -1;
    char line[512],perms[8];unsigned long start,end;int found=0;
    while(fgets(line,sizeof(line),file))if(sscanf(line,"%lx-%lx %4s",&start,&end,perms)==3 && address>=start && address+4096<=end) {
        if(perms[0]=='r' && perms[3]=='p'){hook_page=(void *)address;hook_protection=1|(perms[1]=='w'?2:0)|(perms[2]=='x'?4:0);found=1;}break;
    }
    fclose(file);return found?0:-1;
}
int runtime_bind(void) {
    if(installed)return 0;
    const struct dashborne_host *api=runtime_host;
    if(!api || api->abi_version!=DASHBORNE_EXTENSION_ABI || api->struct_size<sizeof(*api) || !api->resolve || !api->log || host)return -1;
    host=api;hook_count=0;
    if(!fingerprint("/res/lib/libzkgui.so",3464120u,0x9de99542u) ||
       !fingerprint("/lib/libeasyui.so",826052u,0xece0feecu))return -1;
#define BIND(v,handle,s) do {*(void **)(&(v))=api->resolve(handle,s);if(!(v)){api->log(api,"widgets: missing " s);return -1;}}while(0)
#define UI(v,s) BIND(v,api->ui_runtime,s)
#define APP(v,s) BIND(v,api->official_library,s)
    APP(activity_table,"_ZTV16dash1actActivity");
    APP(stock_create,"_ZN16dash1actActivity8onCreateEv");APP(stock_timer,"_ZN16dash1actActivity7onTimerEi");
    APP(stock_timer_secondary,"_ZThn4_N16dash1actActivity7onTimerEi");
    APP(stock_pause,"_ZN16dash1actActivity7onPauseEv");APP(stock_resume,"_ZN16dash1actActivity8onResumeEv");
    APP(stock_destroy,"_ZN16dash1actActivityD1Ev");APP(stock_delete,"_ZN16dash1actActivityD0Ev");
    APP(stock_click,"_ZN16dash1actActivity7onClickEP6ZKBase");
    UI(register_timer,"_ZN7BaseApp13registerTimerEii");UI(unregister_timer,"_ZN7BaseApp15unregisterTimerEi");
#undef APP
#undef UI
#undef BIND
    if(registry_bind(api) || views_bind(api))return -1;
    if(add_hook(2,(void *)stock_destroy,(void *)destroy,0) || add_hook(3,(void *)stock_delete,(void *)deleting,0) ||
       add_hook(4,(void *)stock_click,(void *)clicked,1) || add_hook(6,(void *)stock_create,(void *)created,0) ||
       add_hook(7,(void *)stock_timer,(void *)timer,1) || add_hook(9,(void *)stock_resume,(void *)resume_view,0) ||
       add_hook(10,(void *)stock_pause,(void *)pause_view,0) || add_hook(24,(void *)stock_timer_secondary,(void *)timer_secondary,1))return -1;
    *(void **)(&stock_click)=hooks[2].before;
    *(void **)(&stock_timer)=hooks[4].before;
    *(void **)(&stock_timer_secondary)=hooks[7].before;
    const int offsets[]={4,24,28,32,36,40,44,48};
    for(int n=0;n<8;++n)for(int deleting_flag=0;deleting_flag<2;++deleting_flag) {
        char symbol[80];snprintf(symbol,sizeof(symbol),"_ZThn%d_N16dash1actActivityD%dEv",offsets[n],deleting_flag?0:1);
        void *expected=api->resolve(api->official_library,symbol);int found=0;
        if(!expected)return -1;
        for(int i=20;i<TABLE_WORDS;++i)if(activity_table[i]==expected) {
            if(add_hook(i,expected,deleting_flag?(void *)delete_secondary:(void *)destroy_secondary,0))return -1;
            ++found;
        }
        if(found!=1)return -1;
    }
    if(map_table() || mprotect(hook_page,4096,hook_protection|2))return -1;
    for(int i=0;i<hook_count;++i)activity_table[hooks[i].index]=hooks[i].after;
    installed=1;
    if(mprotect(hook_page,4096,hook_protection))return -1;
    api->log(api,"widgets: native child views ready");return 0;
}
int runtime_unbind(void) {
    if(!host)return 0;
    /* Native widget pools and editor state remain live until Activity teardown. */
    if(live)return -1;
    if(views_detach())return -1;
    if(installed) {
        for(int i=0;i<hook_count;++i)if(activity_table[hooks[i].index]!=hooks[i].after && activity_table[hooks[i].index]!=hooks[i].before)return -1;
        if(mprotect(hook_page,4096,hook_protection|2))return -1;
        for(int i=0;i<hook_count;++i)activity_table[hooks[i].index]=hooks[i].before;
        if(mprotect(hook_page,4096,hook_protection))return -1;
    }
    if(views_cleanup() || registry_cleanup())return -1;
    installed=0;live=0;host=0;hook_count=0;return 0;
}
