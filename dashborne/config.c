/* Root-card configuration; bounded parsing and atomic replacement. */
#include "config.h"
typedef __SIZE_TYPE__ usize;
extern int open(const char *,int,...),read(int,void *,unsigned int),write(int,const void *,unsigned int),close(int);
extern int fsync(int),rename(const char *,const char *),unlink(const char *);
extern void *malloc(usize);
extern void free(void *);
#ifdef __APPLE__
extern int *__error(void);
#define ERROR_NUMBER (*__error())
#else
extern int *__errno_location(void);
#define ERROR_NUMBER (*__errno_location())
#endif
#ifndef CONFIG_CARD
#define CONFIG_CARD "/mnt/extsd"
#endif
#ifndef CONFIG_LEGACY
#define CONFIG_LEGACY CONFIG_CARD "/Dashborne"
#endif
#define PATH CONFIG_CARD "/dashborne.conf"
static char document[CONFIG_LIMIT+1];
static unsigned int used;
static int valid;
static unsigned int len(const char *s) {unsigned int n=0;while(s[n])++n;return n;}
static int equal(const char *a,const char *b) {while(*a && *a==*b){++a;++b;}return *a==*b;}
static void wipe(void *p,unsigned int n) {volatile char *s=p;while(n--)*s++=0;}
void config_clear(void) {wipe(document,sizeof(document));used=0;valid=0;}
/* Return 1 for complete data, 0 only for absence, -1 for errors/oversize. */
static int load(const char *path,char *out,unsigned int cap,unsigned int *size) {
    int fd=open(path,0);*size=0;if(fd<0)return ERROR_NUMBER==2 ? 0 : -1;
    unsigned int n=0;int got=0;
    while(n<cap && (got=read(fd,out+n,cap-n))>0)n+=(unsigned int)got;
    int ok=got>=0 && n<cap; if(close(fd))ok=0;
    if(!ok){wipe(out,cap);return -1;}out[n]=0;*size=n;return 1;
}
static int key_char(char c) {return (c>='a'&&c<='z') || (c>='A'&&c<='Z') || (c>='0'&&c<='9') || c=='.' || c=='_' || c=='-';}
static int key_ok(const char *s) {unsigned int n=0;while(s[n]){if(!key_char(s[n]))return 0;++n;}return n>0 && n<96;}
/* A line's value is literal: spaces, #, =, quotes and backslashes are data. */
static int parse_line(unsigned int begin,unsigned int end,char key[96],unsigned int *value,unsigned int *length) {
    if(end>begin && document[end-1]=='\r')--end;
    while(begin<end && (document[begin]==' ' || document[begin]=='\t'))++begin;
    if(begin==end || document[begin]=='#' || document[begin]==';')return 0;
    unsigned int n=0;
    while(begin<end && key_char(document[begin])) {if(n>=95)return -1;key[n++]=document[begin++];}
    key[n]=0;if(!n)return -1;
    while(begin<end && (document[begin]==' ' || document[begin]=='\t'))++begin;
    if(begin==end || document[begin++]!='=')return -1;
    *value=begin;*length=end-begin;
    return *length<CONFIG_VALUE ? 1 : -1;
}
static int lookup(const char *wanted,unsigned int *begin,unsigned int *end,unsigned int *value,unsigned int *length) {
    for(unsigned int at=0;at<used;) {
        unsigned int next=at;while(next<used && document[next]!='\n')++next;
        char key[96];unsigned int v=0,n=0;int result=parse_line(at,next,key,&v,&n);
        if(result==1 && equal(key,wanted)) {*begin=at;*end=next<used?next+1:next;*value=v;*length=n;return 1;}
        at=next<used ? next+1 : next;
    }
    return 0;
}
static int validate(void) {
    for(unsigned int i=0;i<used;++i)if(!document[i] || (document[i]=='\r' && (i+1==used || document[i+1]!='\n')))return 0;
    for(unsigned int at=0;at<used;) {
        unsigned int next=at;while(next<used && document[next]!='\n')++next;
        char key[96];unsigned int v,n,b,e,p,l;
        int result=parse_line(at,next,key,&v,&n);if(result<0)return 0;
        if(result && (!lookup(key,&b,&e,&p,&l) || b!=at))return 0; /* duplicate keys */
        at=next<used ? next+1 : next;
    }
    unsigned int b,e,v,n;
    return lookup("format",&b,&e,&v,&n) && n==1 && document[v]=='1';
}
int config_get(const char *key,char *out,unsigned int capacity) {
    if(!capacity)return 0;
    out[0]=0;unsigned int b,e,v,n;
    if(!valid || !lookup(key,&b,&e,&v,&n) || n>=capacity)return 0;
    for(unsigned int i=0;i<n;++i)out[i]=document[v+i];
    out[n]=0;return 1;
}
int config_set(const char *key,const char *value) {
    if(!valid || !key_ok(key))return 0;
    unsigned int n=len(value),k=len(key);if(n>=CONFIG_VALUE)return 0;
    for(unsigned int i=0;i<n;++i)if(value[i]=='\n' || value[i]=='\r')return 0;
    unsigned int b=used,e=used,v=0,l=0;int found=lookup(key,&b,&e,&v,&l);
    unsigned int separator=!found && used && document[used-1]!='\n';
    unsigned int size=used-(e-b)+separator+k+1+n+1;if(size>CONFIG_LIMIT)return 0;
    char *next=malloc(size+1);if(!next)return 0;
    unsigned int at=0;for(unsigned int i=0;i<b;++i)next[at++]=document[i];
    if(separator)next[at++]='\n';
    for(unsigned int i=0;i<k;++i)next[at++]=key[i];
    next[at++]='=';
    for(unsigned int i=0;i<n;++i)next[at++]=value[i];
    next[at++]='\n';
    for(unsigned int i=e;i<used;++i)next[at++]=document[i];
    next[at]=0;
    wipe(document,sizeof(document));for(unsigned int i=0;i<=at;++i)document[i]=next[i];used=at;
    wipe(next,size+1);free(next);return 1;
}
int config_commit(void) {
    if(!valid || !validate())return 0;
    int fd=open(PATH ".next",1|0100|01000,0600);if(fd<0)return 0;
    unsigned int at=0;while(at<used){int n=write(fd,document+at,used-at);if(n<=0)break;at+=(unsigned int)n;}
    int ok=at==used && fsync(fd)==0;if(close(fd))ok=0;
    if(ok && rename(PATH ".next",PATH)==0)return 1;
    unlink(PATH ".next");return 0;
}
static int legacy_settings(void) {
    char *raw=malloc(8193);if(!raw)return 0;unsigned int size=0;
    int result=load(CONFIG_LEGACY "/settings.conf",raw,8193,&size),ok=result>=0;
    if(result==0)ok=config_set("tweak.dashborne-badge.enabled","1");
    if(result==1)for(unsigned int i=0;i<size;++i)if(!raw[i])ok=0;
    if(result==1 && ok)for(unsigned int at=0;at<size;) {
        unsigned int end=at;while(end<size && raw[end]!='\n')++end;
        unsigned int stop=end;if(stop>at && raw[stop-1]=='\r')--stop;raw[stop]=0;
        char *line=raw+at;
        if(equal(line,"wifi_adb=1"))ok=ok && config_set("adb.enabled","1");
        else if(equal(line,"wifi_adb=0"))ok=ok && config_set("adb.enabled","0");
        else if(stop-at>8 && line[0]=='p'&&line[1]=='a'&&line[2]=='t'&&line[3]=='c'&&line[4]=='h'&&line[5]=='.') {
            char *value=line+6;while(*value && *value!='=')++value;
            if(*value){*value++=0;if(key_ok(line+6) && (equal(value,"0")||equal(value,"1"))) {
                char key[112]="tweak.";unsigned int i=6;
                for(char *p=line+6;*p && i<100;++p)key[i++]=*p;
                const char *suffix=".enabled";while(*suffix)key[i++]=*suffix++;key[i]=0;
                ok=ok && config_set(key,value);
            }}
        }
        at=end<size ? end+1 : end;
    }
    wipe(raw,8193);free(raw);return ok;
}
static int credentials(const char *ssid,const char *pass) {
    unsigned int a=len(ssid),b=len(pass);if(a<1 || a>32 || b<8 || b>63)return 0;
    for(unsigned int i=0;i<a;++i)if(ssid[i]=='\r'||ssid[i]=='\n')return 0;
    for(unsigned int i=0;i<b;++i)if(pass[i]=='\r'||pass[i]=='\n')return 0;
    return 1;
}
static int legacy_wifi(void) {
    char raw[100]={0};unsigned int size=0;
    int result=load(CONFIG_LEGACY "/wifi.conf",raw,sizeof(raw),&size);
    if(!result)result=load(CONFIG_CARD "/adb-enabler/wifi.conf",raw,sizeof(raw),&size);
    if(result<=0)return result==0;
    for(unsigned int i=0;i<size;++i)if(!raw[i]){wipe(raw,sizeof(raw));return 0;}
    unsigned int at=0;while(at<size && raw[at]!='\n')++at;
    if(at==size){wipe(raw,sizeof(raw));return 0;}
    raw[at]=0;if(at && raw[at-1]=='\r')raw[at-1]=0;
    unsigned int end=size;if(end && raw[end-1]=='\n')--end;if(end && raw[end-1]=='\r')--end;raw[end]=0;
    int ok=credentials(raw,raw+at+1) && config_set("wifi.ssid",raw) && config_set("wifi.password",raw+at+1);
    wipe(raw,sizeof(raw));return ok;
}
int config_load(void) {
    config_clear();int result=load(PATH,document,sizeof(document),&used);
    if(result==1){valid=validate();if(!valid)config_clear();return valid;}
    if(result<0){config_clear();return 0;}
    valid=1;
    int ok=config_set("format","1") && config_set("adb.enabled","0") &&
        config_set("wifi.ssid","") && config_set("wifi.password","") &&
        config_set("loader.autolaunch","0") && config_set("loader.app","") &&
        legacy_settings() && legacy_wifi() && config_commit();
    if(!ok)config_clear();
    else {unlink(CONFIG_LEGACY "/settings.conf");unlink(CONFIG_LEGACY "/wifi.conf");}
    return ok;
}
int config_wifi(char ssid[33],char password[64]) {
    ssid[0]=password[0]=0;
    int ok=config_load() && config_get("wifi.ssid",ssid,33) && config_get("wifi.password",password,64) && credentials(ssid,password);
    config_clear();if(!ok){wipe(ssid,33);wipe(password,64);}return ok;
}
