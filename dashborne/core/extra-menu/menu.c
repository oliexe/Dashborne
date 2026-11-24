#include "../services.h"
#include "window.h"
typedef __SIZE_TYPE__ usize;
typedef __UINTPTR_TYPE__ uintptr;
extern int open(const char *,int,...),read(int,void *,unsigned int),close(int);
extern unsigned long crc32(unsigned long,const unsigned char *,unsigned int);
extern int mprotect(void *,usize,int);
extern void *fopen(const char *,const char *);
extern char *fgets(char *,int,void *);
extern int fclose(void *),sscanf(const char *,const char *,...);
static const struct dashborne_host *host;
static void **table,*page;
static int protection,installed;
#define EXTRA_WINDOW 119200
#define EXTRA_TIMER 97401
#define EXTRA_TIMEOUT 4000
static const int indices[]={4,7,17,24,52};
static void *saved[5];
static int (*id_of)(void *);
static void (*original_click)(void *,void *);
static int (*original_touch)(void *,const void *),(*original_touch_secondary)(void *,const void *);
static int (*original_timer)(void *,int),(*original_timer_secondary)(void *,int);
static void *(*find_control)(void *,int);
static int (*is_visible)(void *);
static void (*visible)(void *,int);
static void (*register_timer)(void *,int,int),(*reset_timer)(void *,int,int),(*unregister_timer)(void *,int);
static void hide_entry(void *self) {
    void *control=find_control(self,EXTRA_WINDOW);
    if(control)visible(control,0);
}
static void reveal_entry(void *self) {
    void *control=find_control(self,EXTRA_WINDOW);
    if(!control)return;
    if(!is_visible(control)) {
        visible(control,1);
        register_timer(self,EXTRA_TIMER,EXTRA_TIMEOUT);
    }
    reset_timer(self,EXTRA_TIMER,EXTRA_TIMEOUT);
}
/* The Activity already registers this listener only while the dashboard is
   active. Keep its return value and every stock touch handler intact. */
static int touched(void *self,const void *event) {
    int result=original_touch(self,event);
    if(event)reveal_entry(self);
    return result;
}
static int touched_secondary(void *self,const void *event) {
    int result=original_touch_secondary(self,event);
    if(event)reveal_entry((unsigned char *)self-10*sizeof(void *));
    return result;
}
static int timer(void *self,int id) {
    if(id!=EXTRA_TIMER)return original_timer(self,id);
    hide_entry(self);
    /* Native timers unregister themselves on false. No polling timer remains. */
    return 0;
}
static int timer_secondary(void *self,int id) {
    if(id!=EXTRA_TIMER)return original_timer_secondary(self,id);
    hide_entry((unsigned char *)self-sizeof(void *));return 0;
}
static void clicked(void *self,void *control) {
    if(control && id_of(control)==299200) {
        unregister_timer(self,EXTRA_TIMER);hide_entry(self);extra_window_open();
    }
    else original_click(self,control);
}
static int fingerprint(const char *path,unsigned int size,unsigned int expected) {
    int fd=open(path,0);if(fd<0)return 0;
    unsigned char buffer[4096];unsigned int count=0;unsigned long crc=0;int n;
    while((n=read(fd,buffer,sizeof(buffer)))>0) {
        if((unsigned int)n>size-count) {close(fd);return 0;}
        count+=(unsigned int)n;crc=crc32(crc,buffer,(unsigned int)n);
    }
    close(fd);return n==0 && count==size && crc==expected;
}
static int map(void) {
    uintptr address=(uintptr)table&~(uintptr)4095;
    if((uintptr)(table+53)>address+4096)return -1;
    void *f=fopen("/proc/self/maps","r");if(!f)return -1;
    char line[512],perms[8];unsigned long start,end;int found=0;
    while(fgets(line,sizeof(line),f))
        if(sscanf(line,"%lx-%lx %4s",&start,&end,perms)==3 && address>=start && address+4096<=end) {
            if(perms[0]=='r' && perms[3]=='p') {
                page=(void *)address;protection=1|(perms[1]=='w'?2:0)|(perms[2]=='x'?4:0);found=1;
            }
            break;
        }
    fclose(f);return found?0:-1;
}
int core_extra_start(const struct dashborne_host *api) {
    if(!api || api->abi_version!=DASHBORNE_EXTENSION_ABI || api->struct_size<sizeof(*api) || host)return -1;
    host=api;
    if(!api->extra_count || !api->extra_entry || !api->open_extra || !api->register_activity ||
       !fingerprint("/res/lib/libzkgui.so",3464120u,0x9de99542u) ||
       !fingerprint("/lib/libeasyui.so",826052u,0xece0feecu))return -1;
#define UI(v,s) do {*(void **)(&(v))=api->resolve(api->ui_runtime,s);if(!(v))return -1;}while(0)
    UI(id_of,"_ZNK6ZKBase5getIDEv");
    UI(find_control,"_ZN7BaseApp15findControlByIDEi");
    UI(visible,"_ZN6ZKBase10setVisibleEb");UI(is_visible,"_ZNK6ZKBase9isVisibleEv");
    UI(register_timer,"_ZN7BaseApp13registerTimerEii");
    UI(reset_timer,"_ZN7BaseApp10resetTimerEii");UI(unregister_timer,"_ZN7BaseApp15unregisterTimerEi");
#undef UI
    table=api->resolve(api->official_library,"_ZTV16dash1actActivity");
    if(!table)return -1;
    const char *symbols[]={"_ZN16dash1actActivity7onClickEP6ZKBase",
        "_ZN16dash1actActivity7onTimerEi","_ZN16dash1actActivity12onTouchEventERK11MotionEvent",
        "_ZThn4_N16dash1actActivity7onTimerEi","_ZThn40_N16dash1actActivity12onTouchEventERK11MotionEvent"};
    for(int i=0;i<5;++i) {
        void *stock=api->resolve(api->official_library,symbols[i]);
        if(!stock || (table[indices[i]]!=stock && (!api->owns_callback || !api->owns_callback(api,table[indices[i]]))))return -1;
        saved[i]=table[indices[i]];
    }
    *(void **)(&original_click)=saved[0];*(void **)(&original_timer)=saved[1];
    *(void **)(&original_touch)=saved[2];*(void **)(&original_timer_secondary)=saved[3];
    *(void **)(&original_touch_secondary)=saved[4];
    if(map() || extra_window_bind(api))return -1;
    if(mprotect(page,4096,protection|2))return -1;
    void *callbacks[]={(void *)clicked,(void *)timer,(void *)touched,(void *)timer_secondary,(void *)touched_secondary};
    for(int i=0;i<5;++i)table[indices[i]]=callbacks[i];
    installed=1;
    if(mprotect(page,4096,protection))return -1;
    api->log(api,"extra-menu: dashboard entry ready");return 0;
}
int core_extra_stop(void) {
    if(extra_window_cleanup())return -1;
    if(installed) {
        void *callbacks[]={(void *)clicked,(void *)timer,(void *)touched,(void *)timer_secondary,(void *)touched_secondary};
        for(int i=0;i<5;++i)if(table[indices[i]]!=callbacks[i] && table[indices[i]]!=saved[i])return -1;
        if(mprotect(page,4096,protection|2))return -1;
        for(int i=4;i>=0;--i)table[indices[i]]=saved[i];
        if(mprotect(page,4096,protection))return -1;
    }
    installed=0;host=0;table=0;page=0;return 0;
}
