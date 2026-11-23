#include "dashborne_extension.h"
#include "memory.h"
#include "window.h"
typedef __SIZE_TYPE__ usize;
typedef __UINTPTR_TYPE__ uintptr;
extern int open(const char *, int, ...), close(int), read(int, void *, unsigned int);
extern unsigned long crc32(unsigned long, const unsigned char *, unsigned int);
extern int mprotect(void *, usize, int);
extern long syscall(long, ...);
extern void *fopen(const char *, const char *);
extern char *fgets(char *, int, void *);
extern int fclose(void *), sscanf(const char *, const char *, ...);
struct image_info { const char *name; void *base; const char *symbol; void *address; };
extern int dladdr(const void *, struct image_info *);
static const struct dashborne_host *host;
static void *page;
static int page_protection;
static int (*started)(void);
static void (*restart)(void);
static struct instruction_patch patch;
static struct mirror_session session;
static void *controller;
static void *(*controller_instance)(void);
static int (*projection_enable)(void),(*link_type)(void),(*manual_off)(void *);
static void (*inhibit)(void *,unsigned int,int),(*request_restart)(unsigned short,unsigned short,int);
static void (*video_hide)(void),(*video_play)(void);
static int (*video_show)(unsigned int,int,int,int,int);
static struct video_status (*video_status)(void);
static int mode(int airplay) {return airplay?instruction_apply(&patch):instruction_restore(&patch);}
static int off(void) {return manual_off(controller);}
static void hold(int value) {inhibit(controller,0x80,value);}
static void mirror_restart(void) {request_restart(1888,384,1);}
static void stock_restart(void) {restart();}
static int show(unsigned int generation,int aspect) {
    struct mirror_rect r=aspect_physical_rect(aspect);
    return video_show(generation,r.left,r.top,r.width,r.height);
}
static const struct session_ops operations={mode,0,off,0,mirror_restart,stock_restart,0,hold,show,0,0};
static void log_message(const char *s) { if (host) host->log(host,s); }
static int fingerprint(const char *path, unsigned int size, unsigned int expected) {
    int fd=open(path,0); if(fd<0)return 0;
    unsigned char buffer[4096]; unsigned int count=0; unsigned long crc=0; int n;
    while((n=read(fd,buffer,sizeof(buffer)))>0) {
        if((unsigned int)n>size-count) {close(fd);return 0;}
        count+=(unsigned int)n; crc=crc32(crc,buffer,(unsigned int)n);
    }
    close(fd);return n==0 && count==size && crc==expected;
}
static int map_page(uintptr address) {
    void *file=fopen("/proc/self/maps","r"); if(!file)return 0;
    char line[512],perms[8]; unsigned long start,end;
    uintptr aligned=address&~(uintptr)4095;
    int valid=0;
    while(fgets(line,sizeof(line),file)) {
        if(sscanf(line,"%lx-%lx %4s",&start,&end,perms)!=3 || aligned<start || aligned+4096>end)continue;
        /* Only a readable, executable private mapping of this verified image. */
        if(perms[0]=='r' && perms[2]=='x' && perms[3]=='p') {
            page=(void *)aligned; page_protection=5|(perms[1]=='w'?2:0); valid=1;
        }
        break;
    }
    fclose(file);return valid;
}
static int protect(int writable) {return mprotect(page,4096,page_protection|(writable?2:0));}
static int flush(void *begin,void *end) {
    /* ARM Linux cacheflush: synchronize the modified instruction on all cores. */
    return (int)syscall(0xf0002L,(uintptr)begin,(uintptr)end,0L);
}
static int activate(const struct dashborne_host *api) {
    if(!api || api->abi_version!=DASHBORNE_EXTENSION_ABI || api->struct_size<sizeof(*api) || host)return -1;
    host=api;
    if(!fingerprint("/res/lib/libzkgui.so",3464120u,0x9de99542u) ||
       !fingerprint("/lib/libeasyui.so",826052u,0xece0feecu) ||
       !fingerprint("/res/lib/liblylinkui.so",50480u,0x30f47c7du) ||
       !fingerprint("/res/bin/lylinkapp_ab",149332u,0x75af0e16u) ||
       !fingerprint("/res/lib/libairplay.so",1415532u,0x0bf80d9eu)) {
        log_message("screen-mirroring: unsupported app/projection fingerprint");return -1;
    }
    *(void **)(&started)=host->resolve(host->official_library,"_ZN2lk17is_lylink_startedEv");
    *(void **)(&restart)=host->resolve(host->official_library,"_ZN7lkvideo13restartLylinkEv");
    struct image_info image;
    if(!started || !restart || !dladdr((void *)restart,&image) || !image.base ||
       (uintptr)restart!=(uintptr)image.base+0xf844cu ||
       (uintptr)started!=(uintptr)image.base+0x132ffcu) return -1;
    if(!api->register_activity || !api->owns_callback)return -1;
#define BIND(var,sym,offset) do { \
    *(void **)(&(var))=host->resolve(host->official_library,sym); \
    if((uintptr)(var)!=(uintptr)image.base+(offset))return -1; \
}while(0)
    BIND(controller_instance,"_ZN17DisplayController8instanceEv",0x115898);
    BIND(inhibit,"_ZN17DisplayController17inhibitDisplayOffENS_11InhibitUserEb",0x11585c);
    BIND(manual_off,"_ZN17DisplayController17getUserDisplayOffEv",0x11584c);
    BIND(projection_enable,"_ZN2lk17enable_projectionEv",0x132f00);
    BIND(link_type,"_ZN2lk15get_lylink_typeEv",0x132a90);
    BIND(request_restart,"_ZN2lk15request_restartEttb",0x1331ac);
    BIND(video_hide,"_ZN2lk10video_hideEv",0x132830);
    BIND(video_play,"_ZN2lk11video_startEv",0x133084);
    BIND(video_show,"_ZN2lk10video_showEjiiii",0x1328e8);
    BIND(video_status,"_ZN2lk16get_video_statusEv",0x132a20);
#undef BIND
    controller=controller_instance();
    if(!controller || (((unsigned char *)controller)[8]&0x80))return -1;
    uintptr address=(uintptr)image.base+0xd10f8u;
    if(!map_page(address) || *(unsigned int *)address!=0xe3a01012u)return -1;
    patch=(struct instruction_patch){(unsigned int *)address,0xe3a01012u,0xe3001400u,0,protect,flush};
    static struct session_ops bound;
    bound=operations;bound.enable=projection_enable;bound.type=link_type;
    bound.hide=video_hide;bound.video=video_status;bound.play=video_play;
    session=(struct mirror_session){.ops=&bound};
    if(window_bind(host,&session) || !host->register_extra ||
       host->register_extra(host,"viewer","Screen Mirroring",
           "Mirror your iPhone. Back restores CarPlay / Android Auto.",window_open))return -1;
    log_message("screen-mirroring: Extra entry ready; stock CarPlay/Android Auto unchanged");
    return 0;
}
static int deactivate(void) {
    if(window_cleanup()) {
        log_message("screen-mirroring: viewer or callback cleanup pending; restart required");return -1;
    }
    if(instruction_restore(&patch))return -1;
    session=(struct mirror_session){0};controller=0;
    host=0;started=0;restart=0;
    return 0;
}
DASHBORNE_EXTENSION_EXPORT const struct dashborne_extension *dashborne_extension_v1(void) {
    static const struct dashborne_extension extension={DASHBORNE_EXTENSION_ABI,sizeof(struct dashborne_extension),"screen-mirroring",activate,deactivate};
    return &extension;
}
