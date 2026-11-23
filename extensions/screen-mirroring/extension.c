#include "dashborne_extension.h"
#include "memory.h"
#include "awake.h"
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
    /* Configure the next official startup, before its projection worker starts. */
    if(started()) {log_message("screen-mirroring: projection already running; reboot into configuration first");return -1;}
    uintptr address=(uintptr)image.base+0xd10f8u;
    if(!map_page(address))return -1;
    patch=(struct instruction_patch){(unsigned int *)address,0xe3a01012u,0xe3001400u,0,protect,flush};
    /* mov r1,#0x12 -> movw r1,#0x400: AirPlay owns the projection session.
       Enabling CarPlay simultaneously lets its auto-reconnect reclaim video.
       The following store writes config.function_mask at offset 0x18. */
    if(instruction_apply(&patch)) {log_message("screen-mirroring: instruction update failed");return -1;}
    if(awake_install(host,image.base)) {log_message("screen-mirroring: sleep inhibitor setup failed");return -1;}
    log_message("screen-mirroring: exclusive AirPlay mode selected; awaiting projection startup");
    return 0;
}
static int deactivate(void) {
    if(awake_restore()) {log_message("screen-mirroring: callback restore pending; restart required");return -1;}
    int changed=patch.touched;
    if(instruction_restore(&patch)) {log_message("screen-mirroring: restore pending; restart required");return -1;}
    /* This uses the app's existing restart queue, never a plugin-owned worker. */
    if(changed && started && started()) restart();
    if(changed)log_message("screen-mirroring: original projection flag restored");
    host=(void *)0; started=(void *)0; restart=(void *)0;
    return 0;
}
DASHBORNE_EXTENSION_EXPORT const struct dashborne_extension *dashborne_extension_v1(void) {
    static const struct dashborne_extension extension={DASHBORNE_EXTENSION_ABI,sizeof(struct dashborne_extension),"screen-mirroring",activate,deactivate};
    return &extension;
}
