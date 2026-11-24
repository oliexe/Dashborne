/* Read-only device inventory. No commands, property writes or network requests. */
#include "device.h"
typedef __SIZE_TYPE__ usize;
extern int open(const char *, int, ...);
extern __PTRDIFF_TYPE__ read(int, void *, usize);
extern int close(int);
extern int snprintf(char *, usize, const char *, ...);
extern void *dlsym(void *, const char *);
extern unsigned long crc32(unsigned long, const unsigned char *, unsigned int);
extern __PTRDIFF_TYPE__ pread64(int, void *, usize, long long);
#ifndef DEVICE_ROOT
#define DEVICE_ROOT ""
#endif
#ifndef DASHBORNE_VERSION
#define DASHBORNE_VERSION "development"
#endif
static struct device_row rows[48];
static unsigned int count;
static void *app;
static int (*property)(const char *, char *, const char *);
static void *(*security_instance)(void);
static int (*security_id)(void *, unsigned char *);
static const char *app_version;
static char scratch[8192];
static unsigned int length(const char *s) { unsigned int n=0;while(s[n])++n;return n; }
static int equal(const char *a,const char *b) { while(*a && *a==*b){++a;++b;}return *a==*b; }
static void copy(char *out,usize cap,const char *s) {
    usize n=0;while(*s && n+1<cap) { unsigned char c=(unsigned char)*s++;out[n++]=c>=32 && c!=127 ? (char)c : ' '; }
    while(n && out[n-1]==' ')--n;
    out[n]=0;
}
static int file(const char *path,char *out,unsigned int cap) {
    char full[512];snprintf(full,sizeof(full),"%s%s",DEVICE_ROOT,path);
    int fd=open(full,0);unsigned int n=0;out[0]=0;if(fd<0)return 0;
    while(n+1<cap) { __PTRDIFF_TYPE__ got=read(fd,out+n,cap-1-n);if(got<0){close(fd);out[0]=0;return 0;}if(!got)break;n+=(unsigned int)got; }
    /* Reject truncated files instead of interpreting partial identifiers. */
    char extra;int complete=read(fd,&extra,1)==0;close(fd);out[n]=0;
    if(!complete){out[0]=0;return 0;}return (int)n;
}
static char *add(const char *name,const char *value) {
    if(count>=sizeof(rows)/sizeof(rows[0]))return rows[0].value;
    struct device_row *r=&rows[count++];r->label=name;copy(r->value,sizeof(r->value),value);
    if(!*r->value)copy(r->value,sizeof(r->value),"Unavailable");return r->value;
}
static void file_row(const char *name,const char *path) { file(path,scratch,sizeof(scratch));add(name,scratch); }
/* Exact keys, bounded lines; no substring matches or shell evaluation. */
static void field(const char *text,const char *key,char delimiter,char *out,usize cap) {
    out[0]=0;unsigned int k=length(key);
    for(const char *p=text;*p;) {
        const char *end=p;while(*end && *end!='\n')++end;
        const char *q=p;while(q<end && (*q==' ' || *q=='\t'))++q;
        unsigned int i=0;while(i<k && q+i<end && q[i]==key[i])++i;
        if(i==k) { q+=k;while(q<end && (*q==' ' || *q=='\t'))++q;
            if(q<end && *q++==delimiter) { while(q<end && (*q==' ' || *q=='\t'))++q;
                usize n=0;while(q<end && n+1<cap)out[n++]=*q++;out[n]=0;return; }
        }
        p=*end ? end+1 : end;
    }
}
static void prop_row(const char *name,const char *key) {
    char value[192]={0};if(property)property(key,value,"");value[sizeof(value)-1]=0;
    if(!*value) {file("/etc/build.prop",scratch,sizeof(scratch));field(scratch,key,'=',value,sizeof(value));}
    add(name,value);
}
/* Only a named top-level quoted scalar is accepted from the app settings. */
static void json_string(const char *s,const char *key,char *out,usize cap) {
    int depth=0;out[0]=0;
    while(*s) {
        if(*s=='{'){++depth;++s;continue;}if(*s=='}'){--depth;++s;continue;}
        if(*s!='"'){++s;continue;}
        char token[128];usize n=0;int escaped=0;++s;
        while(*s && *s!='"') {if(*s=='\\'){escaped=1;if(s[1])++s;}if(n+1<sizeof(token))token[n++]=*s;++s;}
        token[n]=0;if(!*s)return;++s;
        if(depth!=1 || escaped || !equal(token,key))continue;
        while(*s==' ' || *s=='\t' || *s=='\r' || *s=='\n')++s;
        if(*s!=':')return;
        ++s;
        while(*s==' ' || *s=='\t' || *s=='\r' || *s=='\n')++s;
        if(*s!='"')return;
        ++s;
        n=0;while(*s && *s!='"') {if(*s=='\\' || (unsigned char)*s<32 || n+1>=cap){out[0]=0;return;}out[n++]=*s++;}
        if(*s!='"'){out[0]=0;return;}out[n]=0;return;
    }
}
static int decimal(const char *s,unsigned long *value) {
    unsigned long v=0;int digits=0;
    while(*s==' ' || *s=='\t')++s;
    while(*s>='0' && *s<='9') {unsigned long d=(unsigned long)(*s++-'0');if(v>(~0ul-d)/10)return 0;v=v*10+d;digits=1;}
    if(*s && *s!=' ' && *s!='\t' && *s!='\n' && *s!='.')return 0;
    *value=v;return digits;
}
static void identify_app(void) {
    if(app_version)return;
    char path[512];snprintf(path,sizeof(path),"%s/res/lib/libzkgui.so",DEVICE_ROOT);
    int fd=open(path,0);if(fd<0)return;
    unsigned long crc=0;unsigned int size=0;__PTRDIFF_TYPE__ n;
    while((n=read(fd,scratch,sizeof(scratch)))>0) {size+=(unsigned int)n;crc=crc32(crc,(unsigned char *)scratch,(unsigned int)n);if(size>32u*1024u*1024u)break;}
    close(fd);
    app_version="Unrecognized build";
    if(n==0 && size==3464120u) {
        if(crc==0x9de99542ul)app_version="6.11.2";
        else if(crc==0xed0e1aa4ul)app_version="6.11.4";
    }
}
void device_bind(void *stock,void *runtime) {
    app=stock;
    *(void **)(&property)=dlsym(runtime,"_ZN16SystemProperties9getStringEPKcPcS1_");
    *(void **)(&security_instance)=dlsym(runtime,"_ZN15SecurityManager11getInstanceEv");
    *(void **)(&security_id)=dlsym(runtime,"_ZN15SecurityManager8getDevIDEPh");
}
/* Read a bounded snapshot through the kernel: a stock worker may replace its
   C++ string allocation while this page is open. Never dereference that pointer.
   A changed/unmapped snapshot is simply unavailable until the next refresh. */
static void component_string(int fd, const char *symbol, char out[192]) {
    struct string_header { unsigned int address, size; } before, after;
    out[0]=0;void *object=dlsym(app,symbol);if(fd<0 || !object)return;
    long long offset=(long long)(__UINTPTR_TYPE__)object;
    if(pread64(fd,&before,sizeof(before),offset)!=(__PTRDIFF_TYPE__)sizeof(before) ||
       !before.address || !before.size || before.size>=192)return;
    char second[192];
    if(pread64(fd,out,before.size,before.address)!=(__PTRDIFF_TYPE__)before.size ||
       pread64(fd,second,before.size,before.address)!=(__PTRDIFF_TYPE__)before.size ||
       pread64(fd,&after,sizeof(after),offset)!=(__PTRDIFF_TYPE__)sizeof(after) ||
       before.address!=after.address || before.size!=after.size) {out[0]=0;return;}
    for(unsigned int i=0;i<before.size;++i)if(out[i]!=second[i] || (unsigned char)out[i]<32 || (unsigned char)out[i]>126){out[0]=0;return;}
    out[before.size]=0;
}
static void component_versions(void) {
    char comms[192]="",ble[192]="",mac[192]="";
    /* These symbols use the inspected 32-bit libstdc++ string ABI. */
    if(app && app_version && (equal(app_version,"6.11.2") || equal(app_version,"6.11.4"))) {
        int fd=open(DEVICE_ROOT "/proc/self/mem",0);
        component_string(fd,"_ZN10InitSystem14enhcom_versionB5cxx11E",comms);
        component_string(fd,"_ZN10InitSystem14goc_fw_versionB5cxx11E",ble);
        component_string(fd,"_ZN10InitSystem12goc_mac_addrB5cxx11E",mac);
        if(fd>=0)close(fd);
    }
    add("Communication firmware",comms);add("BLE firmware",ble);add("BLE MAC address",mac);
}
static void identifiers(const unsigned char *uid,int size,const char *date,char id[192],char serial[192]) {
    id[0]=serial[0]=0;if(size!=16)return;
    unsigned int words[4],sum=0;
    for(unsigned int i=0;i<4;++i) {
        words[i]=(unsigned int)uid[i*4]|((unsigned int)uid[i*4+1]<<8)|
                 ((unsigned int)uid[i*4+2]<<16)|((unsigned int)uid[i*4+3]<<24);
        sum+=words[i];
    }
    if(!(words[0]|words[1]|words[2]|words[3]))return;
    snprintf(id,192,"%08X-%08X-%08X-%08X",words[0],words[1],words[2],words[3]);
    if(length(date)<8)return;
    for(int i=0;i<8;++i)if(date[i]<'0' || date[i]>'9')return;
    /* Stock Settings formats the DASH prefix, UUID word sum, and first eight
       production-stamp digits. Keep this separate from the complete UUID. */
    snprintf(serial,192,"44415348.%08X.%.8s",sum,date);
}
void device_refresh(void) {
    count=0;identify_app();
    add("Dashborne",DASHBORNE_VERSION);
    add("Official dashboard",app_version ? app_version : "Unavailable");
    prop_row("System firmware","ro.system.version");prop_row("Operating system","ro.build.version.release");
    prop_row("UI runtime","ro.easyui.version");
    char date[32]="",name[192]="";
    file("/data/zkapp.cfg",scratch,sizeof(scratch));json_string(scratch,"proddate",date,sizeof(date));json_string(scratch,"dev_name",name,sizeof(name));
    add("Device name",name);
    unsigned char uid[64]={0};char id[192]="",serial[192]="";
    if(security_instance && security_id) {
        void *security=security_instance();int n=security ? security_id(security,uid) : 0;
        identifiers(uid,n,date,id,serial);
    }
    add("DASH serial",serial);add("Hardware ID",id);add("Production stamp",date);
    component_versions();
    prop_row("Board model","ro.product.model");prop_row("Board vendor","ro.product.manufacturer");prop_row("Board name","ro.product.board");
    file_row("Device tree model","/sys/firmware/devicetree/base/model");
    prop_row("OS build date","ro.build.date");prop_row("OS build revision","ro.build.git");
    file_row("Linux kernel","/proc/sys/kernel/osrelease");
    file("/proc/cpuinfo",scratch,sizeof(scratch));char value[192];field(scratch,"model name",':',value,sizeof(value));add("Processor",value);
    file_row("Online CPU cores","/sys/devices/system/cpu/online");
    file("/proc/meminfo",scratch,sizeof(scratch));
    const char *keys[2]={"MemTotal","MemAvailable"},*labels[2]={"Usable RAM","Available RAM"};
    for(int i=0;i<2;++i) {unsigned long kb;field(scratch,keys[i],':',value,sizeof(value));
        if(decimal(value,&kb))snprintf(value,sizeof(value),"%lu.%lu MiB",kb/1024,(kb%1024)*10/1024);else value[0]=0;add(labels[i],value);}
    file("/proc/uptime",scratch,sizeof(scratch));unsigned long seconds;
    if(decimal(scratch,&seconds))snprintf(value,sizeof(value),"%lu days, %lu h %lu min",seconds/86400,(seconds/3600)%24,(seconds/60)%60);else value[0]=0;
    add("Uptime",value);
    file_row("Panel mode","/sys/class/graphics/fb0/modes");file_row("Wi-Fi MAC address","/sys/class/net/wlan0/address");
    /* Identify the media by type, rather than assuming fixed mmc numbering. */
    for(int i=0;i<4;++i) {
        char path[160],type[32];snprintf(path,sizeof(path),"/sys/block/mmcblk%d/device/type",i);file(path,type,sizeof(type));copy(type,sizeof(type),type);
        const char *prefix=equal(type,"MMC") ? "eMMC" : equal(type,"SD") ? "microSD" : (const char *)0;if(!prefix)continue;
        const char *fields[3]={"name","serial","cid"};
        for(int j=0;j<3;++j) {
            /* Labels outlive this stack frame via a fixed table below. */
            static const char *names[2][3]={{"eMMC model","eMMC serial","eMMC CID"},{"microSD model","microSD serial","microSD CID"}};
            snprintf(path,sizeof(path),"/sys/block/mmcblk%d/device/%s",i,fields[j]);file_row(names[equal(type,"SD")][j],path);}
        snprintf(path,sizeof(path),"/sys/block/mmcblk%d/size",i);file(path,scratch,sizeof(scratch));unsigned long sectors;
        if(decimal(scratch,&sectors))snprintf(value,sizeof(value),"%lu.%lu GiB",sectors/2097152,(sectors%2097152)*10/2097152);else value[0]=0;
        add(equal(type,"SD") ? "microSD capacity" : "eMMC capacity",value);
    }
}
unsigned int device_count(void) {return count;}
const struct device_row *device_row(unsigned int index) {return index<count ? &rows[index] : (const struct device_row *)0;}
