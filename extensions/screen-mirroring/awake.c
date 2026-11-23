#include "awake.h"
typedef __UINTPTR_TYPE__ uintptr;
extern int mprotect(void *, __SIZE_TYPE__, int);
extern void *fopen(const char *, const char *);
extern char *fgets(char *, int, void *);
extern int fclose(void *), sscanf(const char *, const char *, ...);

/* Verified 6.11.2 uses inhibitor bits 1, 2 and 4. Own only this unused bit. */
#define MIRROR_INHIBIT 0x80u
static const struct dashborne_host *api;
static void **table, *page, *controller;
static int protection, installed, resumed, held;
static void *(*instance)(void);
static void (*inhibit)(void *, unsigned int, int);
static int (*user_off)(void *), (*link_type)(void), (*video_shown)(void);
static int (*original_timer)(void *, int);
static void (*original_resume)(void *), (*original_pause)(void *);

static void hold(int enable) {
    if(enable==held)return;
    inhibit(controller,MIRROR_INHIBIT,enable);
    held=enable;
    api->log(api,enable?"screen-mirroring: visible AirPlay inhibits idle sleep":
                        "screen-mirroring: idle sleep inhibition released");
}
static void update(void) {
    hold(resumed && !user_off(controller) && link_type()==10 && video_shown());
}
static int timer(void *self,int id) {
    /* The official timer can evaluate idle sleep: inhibit before delegating. */
    update();
    int result=original_timer(self,id);
    update();
    return result;
}
static int timer_secondary(void *self,int id) {
    /* ARM ITimerListener is the secondary base at +4, regardless of host ABI. */
    return timer((char *)self-4,id);
}
static void resume(void *self) {
    original_resume(self);
    resumed=1;
    update();
}
static void pause(void *self) {
    resumed=0;
    hold(0);
    original_pause(self);
}
struct hook {unsigned int slot, address; const char *symbol; void *replacement, *saved;};
static struct hook hooks[]={
    {7,0x112934,"_ZN16dash1actActivity7onTimerEi",(void *)timer,0},
    {9,0x10eff0,"_ZN16dash1actActivity8onResumeEv",(void *)resume,0},
    {10,0x10e738,"_ZN16dash1actActivity7onPauseEv",(void *)pause,0},
    {24,0x113134,"_ZThn4_N16dash1actActivity7onTimerEi",(void *)timer_secondary,0}
};
#define HOOK_COUNT (sizeof(hooks)/sizeof(hooks[0]))
static int map_table(void) {
    uintptr first=(uintptr)table&~(uintptr)4095;
    if(((uintptr)(table+24)&~(uintptr)4095)!=first)return -1;
    void *f=fopen("/proc/self/maps","r");
    if(!f)return -1;
    char line[512],perms[8]; unsigned long start,end;
    int found=0;
    while(fgets(line,sizeof(line),f)) {
        if(sscanf(line,"%lx-%lx %4s",&start,&end,perms)!=3 || first<start || first+4096>end)continue;
        if(perms[0]=='r' && perms[3]=='p') {
            protection=1|(perms[1]=='w'?2:0)|(perms[2]=='x'?4:0);
            page=(void *)first; found=1;
        }
        break;
    }
    fclose(f);
    return found?0:-1;
}
int awake_install(const struct dashborne_host *host,void *image_base) {
    if(api || !host || !image_base)return -1;
    api=host;
#define BIND(var,sym,offset) do { \
    *(void **)(&(var))=api->resolve(api->official_library,sym); \
    if((uintptr)(var)!=(uintptr)image_base+(offset))return -1; \
} while(0)
    BIND(instance,"_ZN17DisplayController8instanceEv",0x115898);
    BIND(inhibit,"_ZN17DisplayController17inhibitDisplayOffENS_11InhibitUserEb",0x11585c);
    BIND(user_off,"_ZN17DisplayController17getUserDisplayOffEv",0x11584c);
    BIND(link_type,"_ZN2lk15get_lylink_typeEv",0x132a90);
    BIND(video_shown,"_ZN7lkvideo12isVideoShownEv",0xf26e4);
    BIND(table,"_ZTV16dash1actActivity",0x350d98);
#undef BIND
    controller=instance();
    if(!controller || (((unsigned char *)controller)[8]&MIRROR_INHIBIT))return -1;
    for(unsigned int i=0;i<HOOK_COUNT;++i) {
        void *expected=api->resolve(api->official_library,hooks[i].symbol);
        if((uintptr)expected!=(uintptr)image_base+hooks[i].address || table[hooks[i].slot]!=expected)return -1;
        hooks[i].saved=expected;
    }
    *(void **)(&original_timer)=hooks[0].saved;
    *(void **)(&original_resume)=hooks[1].saved;
    *(void **)(&original_pause)=hooks[2].saved;
    if(map_table())return -1;
    if(mprotect(page,4096,protection|2))return -1;
    /* Activation, these callbacks and cleanup all run on the UI thread. */
    for(unsigned int i=0;i<HOOK_COUNT;++i)table[hooks[i].slot]=hooks[i].replacement;
    installed=1;
    if(mprotect(page,4096,protection))return -1;
    return 0;
}
int awake_restore(void) {
    if(held)hold(0);
    resumed=0;
    if(installed) {
        for(unsigned int i=0;i<HOOK_COUNT;++i)
            if(table[hooks[i].slot]!=hooks[i].replacement && table[hooks[i].slot]!=hooks[i].saved)return -1;
        if(mprotect(page,4096,protection|2))return -1;
        for(unsigned int i=0;i<HOOK_COUNT;++i)table[hooks[i].slot]=hooks[i].saved;
        /* Keep installed set on permission failure: cleanup can retry safely. */
        if(mprotect(page,4096,protection))return -1;
        installed=0;
    }
    api=0; controller=0; table=0; page=0;
    return 0;
}
