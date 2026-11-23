#include "dashborne_extension.h"
#include "display.h"
#include "../../app/injector/patch.h"
typedef __SIZE_TYPE__ usize;
typedef __UINTPTR_TYPE__ uintptr;
extern void *malloc(usize);
extern void free(void *);
extern int open(const char *, int, ...), close(int), fsync(int);
extern int read(int, void *, unsigned int), write(int, const void *, unsigned int);
extern int rename(const char *, const char *);
extern int snprintf(char *, usize, const char *, ...);
extern unsigned long crc32(unsigned long, const unsigned char *, unsigned int);
extern int mprotect(void *, usize, int);
extern void *fopen(const char *, const char *);
extern char *fgets(char *, int, void *);
extern int fclose(void *);
extern int sscanf(const char *, const char *, ...);
#define TOPICS 80001
#define STOCK_PANEL 31001
#define PANEL 119090
#define ENABLE 299090
#define RESET 299091
#define CONTRAST 919091
#define LUMINANCE 919092
static const struct dashborne_host *host;
static struct display_state display;
static int selected, rendering, installed, original_count;
static const char *status;
static void **vtable, *saved[69];
struct page { void *address; int prot; };
static struct page pages[2];
static int page_count;
static void *(*find)(void *, int);
static int (*id_of)(void *);
static void (*text)(void *, const char *), (*visible)(void *, int), (*select_control)(void *, int);
static void (*refresh)(void *), (*progress)(void *, int), (*seek_listener)(void *, void *);
static void (*original_create)(void *), (*original_click)(void *, void *);
static int (*original_items)(void *, void *);
static void (*original_data)(void *, void *, void *, int);
static void (*original_item_click)(void *, void *, int, int);
static void (*original_progress)(void *, void *, int);
static void log_message(const char *s) { if (host) host->log(host,s); }
static void label(void *self,int id,const char *s) { void *p=find(self,id); if(p)text(p,s); }
static void show(void *self,int id,int value) { void *p=find(self,id); if(p)visible(p,value); }
static int is_topics(void *list) { return list && id_of(list)==TOPICS; }
static int is_selected(void *self) {
    (void)self; return selected;
}
static void render(void *self) {
    char line[96]; rendering=1;
    snprintf(line,sizeof(line),"Contrast: %d",display.contrast); label(self,599091,line);
    snprintf(line,sizeof(line),"Black level / luminance: %d",display.luminance); label(self,599092,line);
    label(self,ENABLE,display.enabled?"Adjustments enabled":"Enable adjustments");
    label(self,599094,status?status:display.enabled?"Applied to this session":"Original display values");
    void *p=find(self,CONTRAST); if(p)progress(p,display.contrast);
    p=find(self,LUMINANCE); if(p)progress(p,display.luminance);
    p=find(self,ENABLE); if(p)select_control(p,display.enabled);
    rendering=0;
}
static void created(void *self) {
    selected=0;
    original_create(self);
    void *c=find(self,CONTRAST), *l=find(self,LUMINANCE);
    if(c)seek_listener(c,(char *)self+24);
    if(l)seek_listener(l,(char *)self+24);
    render(self);
    log_message(c&&l?"contrast: settings controls ready":"contrast: settings controls missing");
}
static int item_count(void *self,void *list) {
    int n=original_items(self,list);
    if(is_topics(list) && n>=0 && n<64) { original_count=n; return n+1; }
    return n;
}
static void item_data(void *self,void *list,void *item,int index) {
    if(is_topics(list) && index==original_count) {
        text(item,"* Contrast"); select_control(item,is_selected(self)); return;
    }
    original_data(self,list,item,index);
    if(is_topics(list) && is_selected(self)) select_control(item,0);
}
static void item_clicked(void *self,void *list,int index,int sub) {
    if(!is_topics(list)) { original_item_click(self,list,index,sub); return; }
    if(index==original_count) {
        selected=1; show(self,STOCK_PANEL,0); show(self,PANEL,1); render(self);
        refresh(list); log_message("contrast: settings page opened"); return;
    }
    selected=0; show(self,PANEL,0); show(self,STOCK_PANEL,1);
    original_item_click(self,list,index,sub); refresh(list);
}
static void result_status(int result) {
    status=result==-2?"Restore failed - use Reset or restart":result?"Adjustment failed; original values restored":(const char *)0;
    if(result)log_message(result==-2?"contrast: display restore failed":"contrast: adjustment rejected; restored");
}
static void clicked(void *self,void *control) {
    int id=id_of(control);
    if(id==ENABLE) {
        int r=display.enabled?display_restore(&display):display_apply(&display,display.contrast,display.luminance);
        result_status(r?display.touched?-2:-1:0); render(self); return;
    }
    if(id==RESET) {
        int r=display_restore(&display);
        if(!r) { display.contrast=display.baseline_contrast; display.luminance=display.baseline_luminance; }
        result_status(r?-2:0); render(self); return;
    }
    original_click(self,control);
}
static void changed(void *self,void *bar,int value) {
    int id=id_of(bar);
    if(id!=CONTRAST && id!=LUMINANCE) { original_progress(self,bar,value); return; }
    if(rendering || value<0 || value>100) return;
    int c=id==CONTRAST?value:display.contrast, l=id==LUMINANCE?value:display.luminance;
    if(display.enabled) result_status(display_apply(&display,c,l));
    else { display.contrast=c; display.luminance=l; status="Enable adjustments to apply"; }
    render(self);
}
static int items_secondary(void *s,void *l) { return item_count((char *)s-32,l); }
static void data_secondary(void *s,void *l,void *i,int n) { item_data((char *)s-32,l,i,n); }
static void click_secondary(void *s,void *l,int n,int sub) { item_clicked((char *)s-28,l,n,sub); }
static void progress_secondary(void *s,void *b,int v) { changed((char *)s-24,b,v); }

static unsigned char *load(const char *path,unsigned int *size,unsigned int limit) {
    int fd=open(path,0); if(fd<0)return (void *)0;
    unsigned char *buf=malloc(limit+1); unsigned int n=0; int k=0;
    if(buf)while(n<=limit && (k=read(fd,buf+n,limit+1-n))>0)n+=(unsigned int)k;
    close(fd);
    if(!buf || k<0 || n>limit) {free(buf);return (void *)0;}
    *size=n; return buf;
}
static int fingerprint(const char *path,unsigned int size,unsigned int crc) {
    unsigned int n=0; unsigned char *b=load(path,&n,size);
    int ok=b && n==size && crc32(0,b,n)==crc; free(b); return ok;
}
static int overlay(void) {
    unsigned int bn=0,pn=0,on=0;
    char path[512],next[512];
    unsigned char *b=load("/res/ui/awlsettings.ftu",&bn,131072), *p=(void *)0,*out=(void *)0;
    snprintf(path,sizeof(path),"%s/assets/panel.dbp",host->extension_directory);
    p=load(path,&pn,16384); int ok=b&&p&&patch_layout(b,bn,p,pn,&out,&on);
    if(ok) {
        snprintf(path,sizeof(path),"%s/generated.ftu",host->extension_directory);
        snprintf(next,sizeof(next),"%s/generated.ftu.next",host->extension_directory);
        int fd=open(next,1|0100|01000,0644);
        ok=fd>=0;
        if(ok) { unsigned int at=0; while(at<on) {int n=write(fd,out+at,on-at); if(n<=0){ok=0;break;} at+=(unsigned int)n;} if(fsync(fd))ok=0; close(fd); }
        if(ok)ok=rename(next,path)==0;
        if(ok)ok=host->overlay_file(host,"generated.ftu","/res/ui/awlsettings.ftu")==0;
    }
    free(b);free(p);free(out);return ok;
}
static int map_pages(void) {
    uintptr first=(uintptr)vtable&~(uintptr)4095, last=((uintptr)vtable+sizeof(saved)-1)&~(uintptr)4095;
    page_count=last==first?1:2;
    for(int i=0;i<page_count;++i) {
        uintptr addr=first+(uintptr)i*4096; pages[i].address=(void *)addr; pages[i].prot=0;
        void *f=fopen("/proc/self/maps","r"); char line[512],perms[8]; unsigned long a,b;
        if(!f)return 0;
        while(fgets(line,sizeof(line),f))if(sscanf(line,"%lx-%lx %4s",&a,&b,perms)==3 && addr>=a && addr+4096<=b) {
            pages[i].prot=(perms[0]=='r'?1:0)|(perms[1]=='w'?2:0)|(perms[2]=='x'?4:0);break;
        }
        fclose(f);if(!pages[i].prot)return 0;
    }
    return 1;
}
static int protection(int writable) {
    int ok=1;
    for(int i=0;i<page_count;++i)if(mprotect(pages[i].address,4096,pages[i].prot|(writable?2:0)))ok=0;
    return ok;
}
struct hook {int index; const char *symbol; void *replacement;};
static const struct hook hooks[]={
 {4,"_ZN19awlsettingsActivity7onClickEP6ZKBase",(void *)clicked},
 {6,"_ZN19awlsettingsActivity8onCreateEv",(void *)created},
 {12,"_ZN19awlsettingsActivity17onProgressChangedEP9ZKSeekBari",(void *)changed},
 {13,"_ZNK19awlsettingsActivity16getListItemCountEPK10ZKListView",(void *)item_count},
 {14,"_ZN19awlsettingsActivity18obtainListItemDataEP10ZKListViewPNS0_10ZKListItemEi",(void *)item_data},
 {15,"_ZN19awlsettingsActivity11onItemClickEP10ZKListViewii",(void *)item_clicked},
 {30,"_ZThn24_N19awlsettingsActivity17onProgressChangedEP9ZKSeekBari",(void *)progress_secondary},
 {37,"_ZThn28_N19awlsettingsActivity11onItemClickEP10ZKListViewii",(void *)click_secondary},
 {42,"_ZThn32_NK19awlsettingsActivity16getListItemCountEPK10ZKListView",(void *)items_secondary},
 {43,"_ZThn32_N19awlsettingsActivity18obtainListItemDataEP10ZKListViewPNS0_10ZKListItemEi",(void *)data_secondary}
};
#define HOOKS (sizeof(hooks)/sizeof(hooks[0]))
static int activate(const struct dashborne_host *api) {
    if(!api || api->abi_version!=DASHBORNE_EXTENSION_ABI || api->struct_size<sizeof(*api))return -1;
    host=api;
    if(!fingerprint("/res/lib/libzkgui.so",3464120u,0x9de99542u) ||
       !fingerprint("/lib/libeasyui.so",826052u,0xece0feecu)) {
        log_message("contrast: unsupported app/runtime fingerprint");return -1;
    }
#define BIND(var,sym) do { *(void **)(&(var))=host->resolve(host->ui_runtime,sym); if(!(var))return -1; }while(0)
    BIND(find,"_ZN7BaseApp15findControlByIDEi"); BIND(id_of,"_ZNK6ZKBase5getIDEv");
    BIND(text,"_ZN10ZKTextView7setTextEPKc"); BIND(visible,"_ZN6ZKBase10setVisibleEb");
    BIND(select_control,"_ZN6ZKBase11setSelectedEb"); BIND(refresh,"_ZN10ZKListView15refreshListViewEv");
    BIND(progress,"_ZN9ZKSeekBar11setProgressEi");
    BIND(seek_listener,"_ZN9ZKSeekBar24setSeekBarChangeListenerEPNS_22ISeekBarChangeListenerE");
#undef BIND
    vtable=host->resolve(host->official_library,"_ZTV19awlsettingsActivity");
    if(!vtable)return -1;
    for(unsigned int i=0;i<69;++i)saved[i]=vtable[i];
    for(unsigned int i=0;i<HOOKS;++i)if(vtable[hooks[i].index]!=host->resolve(host->official_library,hooks[i].symbol) &&
        (!host->owns_callback || !host->owns_callback(host,vtable[hooks[i].index]))) {
        log_message("contrast: incompatible settings callbacks");return -1;
    }
    *(void **)(&original_click)=saved[4]; *(void **)(&original_create)=saved[6];
    *(void **)(&original_progress)=saved[12]; *(void **)(&original_items)=saved[13];
    *(void **)(&original_data)=saved[14]; *(void **)(&original_item_click)=saved[15];
    if(!map_pages() || display_capture(&display) || !overlay()) {
        log_message("contrast: display or layout unavailable");return -1;
    }
    if(!protection(1)) {protection(0);return -1;}
    for(unsigned int i=0;i<HOOKS;++i)vtable[hooks[i].index]=hooks[i].replacement;
    installed=1;
    if(!protection(0))return -1;
    log_message("contrast: settings entry installed; display unchanged until enabled");
    return 0;
}
static int deactivate(void) {
    if(display_restore(&display)) {log_message("contrast: cleanup could not restore display");return -1;}
    if(installed) {
        for(unsigned int i=0;i<HOOKS;++i)
            if(vtable[hooks[i].index]!=hooks[i].replacement &&
               vtable[hooks[i].index]!=saved[hooks[i].index])return -1;
        if(!protection(1)) {protection(0);return -1;}
        for(unsigned int i=0;i<HOOKS;++i)vtable[hooks[i].index]=saved[hooks[i].index];
        if(!protection(0))return -1;
        installed=0;
    }
    log_message("contrast: original callbacks and display restored");
    host=(void *)0; return 0;
}
DASHBORNE_EXTENSION_EXPORT const struct dashborne_extension *dashborne_extension_v1(void) {
    static const struct dashborne_extension extension={DASHBORNE_EXTENSION_ABI,sizeof(struct dashborne_extension),"contrast",activate,deactivate};
    return &extension;
}
