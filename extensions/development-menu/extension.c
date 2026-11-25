#include "stock_compat.h"
#include "dashborne_extension.h"
typedef __UINTPTR_TYPE__ uintptr;
extern int open(const char *, int, ...), close(int), read(int, void *, unsigned int);
extern unsigned long crc32(unsigned long, const unsigned char *, unsigned int);
extern void *fopen(const char *, const char *);
extern char *fgets(char *, int, void *);
extern int fclose(void *), sscanf(const char *, const char *, ...);
struct image_info { const char *name; void *base; const char *symbol; void *address; };
extern int dladdr(const void *, struct image_info *);
static unsigned int *category_count, previous_count;

static int writable_data(uintptr address) {
    void *f=fopen("/proc/self/maps","r");if(!f)return 0;
    char line[512],perms[8];unsigned long start,end;int valid=0;
    while(fgets(line,sizeof(line),f)) {
        if(sscanf(line,"%lx-%lx %4s",&start,&end,perms)==3 &&
           address>=start && address+sizeof(unsigned int)<=end) {
            valid=perms[0]=='r' && perms[1]=='w' && perms[2]=='-' && perms[3]=='p';
            break;
        }
    }
    fclose(f);return valid;
}
static int activate(const struct dashborne_host *host) {
    if(!host || host->abi_version!=DASHBORNE_EXTENSION_ABI ||
       host->struct_size<sizeof(*host) || !host->resolve || !host->log || category_count)return -1;
    if(!db_stock_version()) {
        host->log(host,"development-menu: unsupported stock app fingerprint");return -1;
    }
    void *load=host->resolve(host->official_library,"_Z22pageDevelopment_LoadUIv");
    struct image_info image;
    if(!load || !dladdr(load,&image) || !image.base ||
       (uintptr)load!=(uintptr)image.base+DB_STOCK_OFFSET(0xed990u,0xf9f44u))return -1;
    /* The native sidebar and page-selection handler share this RAM-only count.
       All six labels, layouts and callbacks already belong to the stock app. */
    unsigned int *count=(unsigned int *)((uintptr)image.base+DB_STOCK_OFFSET(0x359034u,0x3590f4u));
    if(!writable_data((uintptr)count) || (*count!=5 && *count!=6))return -1;
    previous_count=*count;category_count=count;*count=6;
    host->log(host,"development-menu: stock Settings now includes Development");
    return 0;
}
static int deactivate(void) {
    if(category_count) {
        if(*category_count==6)*category_count=previous_count;
        category_count=0;previous_count=0;
    }
    return 0;
}
DASHBORNE_EXTENSION_EXPORT const struct dashborne_extension *dashborne_extension_v1(void) {
    static const struct dashborne_extension extension={
        DASHBORNE_EXTENSION_ABI,sizeof(struct dashborne_extension),"development-menu",activate,deactivate
    };
    return &extension;
}
