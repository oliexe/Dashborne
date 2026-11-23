#include "presentation.h"
typedef __UINTPTR_TYPE__ uintptr;
extern void *fopen(const char *, const char *);
extern char *fgets(char *, int, void *);
extern int fclose(void *), sscanf(const char *, const char *, ...);
extern int snprintf(char *, __SIZE_TYPE__, const char *, ...);
static const struct dashborne_host *api;
static unsigned char *manager;
static void (*refresh)(void *);
static int acquired;
static unsigned char saved_grant;
int presentation_has_widget(void) {
    return manager && *(unsigned int *)(manager+16)!=0;
}
/* 6.11.2 ProjectionManager: widget at +16; resource grant at +24;
   visible state at +25; transition/modal gates at +26/+27.
   AirPlay delivers frames without the CP/AA resource-grant notification. */
void presentation_release(void) {
    if(!acquired)return;
    acquired=0;
    /* Preserve a later stock revocation. Only undo our still-present grant. */
    if(manager[24]==1 && manager[24]!=saved_grant) {
        manager[24]=saved_grant;
        refresh(manager);
    }
    api->log(api,"screen-mirroring: widget resource grant released");
}
void presentation_update(int active) {
    if(!manager)return;
    if(!active || !presentation_has_widget()) {
        presentation_release();
        return;
    }
    if(!acquired) {
        saved_grant=manager[24];
        acquired=1;
        char message[128];
        snprintf(message,sizeof(message),"screen-mirroring: AirPlay acquired widget presentation (UI gates %u/%u)",
                 (unsigned int)manager[26],(unsigned int)manager[27]);
        api->log(api,message);
    }
    int changed=manager[24]!=1;
    manager[24]=1;
    /* Keep the stock transition and modal gates; never force their values.
       The caller has already verified visible, decoded AirPlay video. */
    int desired=manager[26] && manager[27];
    if(changed || manager[25]!=desired)refresh(manager);
}
void presentation_reset(void) {
    presentation_release();
    manager=0; refresh=0; api=0;
}
int presentation_bind(const struct dashborne_host *host,void *image_base) {
    if(manager || !host || !image_base)return -1;
    /* Caller fingerprinted the complete installed official image. These
       private addresses are additionally anchored to the exported UI method. */
    if((uintptr)host->resolve(host->official_library,"_ZN12WdgRefreshUI8updateUIEv") !=
       (uintptr)image_base+0x11db48)return -1;
    unsigned char *candidate=(unsigned char *)image_base+0x35fd60;
    unsigned int *method=(unsigned int *)((char *)image_base+0x11d7c8);
    if(method[0]!=0xe5903010u || method[1]!=0xe92d40f0u)return -1;
    void *f=fopen("/proc/self/maps","r"); if(!f)return -1;
    char line[512],perms[8]; unsigned long start,end; int valid=0;
    while(fgets(line,sizeof(line),f)) {
        if(sscanf(line,"%lx-%lx %4s",&start,&end,perms)==3 &&
           (uintptr)candidate>=start && (uintptr)candidate+28<=end) {
            valid=perms[0]=='r' && perms[1]=='w' && perms[3]=='p';break;
        }
    }
    fclose(f);
    if(!valid)return -1;
    api=host; manager=candidate; refresh=(void (*)(void *))method;
    return 0;
}
