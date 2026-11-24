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
static void **table,*saved,*page;
static int protection,installed;
static int (*id_of)(void *);
static void (*original_click)(void *,void *);
static void clicked(void *self,void *control) {
    if(control && id_of(control)==299200)extra_window_open();
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
    uintptr address=(uintptr)(table+4)&~(uintptr)4095;
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
    *(void **)(&id_of)=api->resolve(api->ui_runtime,"_ZNK6ZKBase5getIDEv");
    table=api->resolve(api->official_library,"_ZTV16dash1actActivity");
    void *stock=api->resolve(api->official_library,"_ZN16dash1actActivity7onClickEP6ZKBase");
    if(!id_of || !table || !stock || (table[4]!=stock && !api->owns_callback(api,table[4])))return -1;
    saved=table[4];*(void **)(&original_click)=saved;
    if(map() || extra_window_bind(api))return -1;
    if(mprotect(page,4096,protection|2))return -1;
    table[4]=(void *)clicked;installed=1;
    if(mprotect(page,4096,protection))return -1;
    api->log(api,"extra-menu: dashboard entry ready");return 0;
}
int core_extra_stop(void) {
    if(extra_window_cleanup())return -1;
    if(installed) {
        if(table[4]!=(void *)clicked && table[4]!=saved)return -1;
        if(mprotect(page,4096,protection|2))return -1;
        table[4]=saved;
        if(mprotect(page,4096,protection))return -1;
    }
    installed=0;host=0;table=0;page=0;return 0;
}
