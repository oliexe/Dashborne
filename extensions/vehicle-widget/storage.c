#include "storage.h"
#include "../../dashborne/config.h"
extern int open(const char *,int,...),close(int),read(int,void *,unsigned int),write(int,const void *,unsigned int),unlink(const char *),fsync(int),rename(const char *,const char *);
extern int snprintf(char *,__SIZE_TYPE__,const char *,...);
/* Hex chunks preserve JSON whitespace and avoid config escaping ambiguities. */
#define LIMIT 12000
#define CHUNK 480
#ifndef LAYOUT_STOCK
#define LAYOUT_STOCK "/data/pagelayout.json"
#endif
#ifndef LAYOUT_RAM
#define LAYOUT_RAM VEHICLE_LAYOUT_RAM
#endif
static unsigned char bytes[LIMIT+1];
static int read_file(const char *p) {
    int fd=open(p,0);if(fd<0)return -1;int n=0,r=0;
    while(n<=LIMIT && (r=read(fd,bytes+n,(unsigned int)(LIMIT+1-n)))>0)n+=r;
    if(close(fd) || r<0 || n>LIMIT || !n)return -1;
    return n;
}
static int digit(char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;return -1;}
static int write_ram(int n) {
    int fd=open(LAYOUT_RAM ".next",1|0100|01000,0600);if(fd<0)return -1;int at=0;
    while(at<n){int r=write(fd,bytes+at,(unsigned int)(n-at));if(r<=0)break;at+=r;}
    int ok=at==n && !fsync(fd);if(close(fd))ok=0;
    if(ok && !rename(LAYOUT_RAM ".next",LAYOUT_RAM))return 0;
    unlink(LAYOUT_RAM ".next");return -1;
}
int layout_prepare(void) {
    if(!config_load())return -1;
    char value[1024],key[80];int n=0;
    if(config_get("vehicle-widget.layout.bytes",value,sizeof(value))) {
        for(int i=0;value[i];++i){if(value[i]<'0'||value[i]>'9'||n>LIMIT/10){config_clear();return -1;}n=n*10+value[i]-'0';}
        if(n<1||n>LIMIT){config_clear();return -1;}
        for(int at=0;at<n;at+=CHUNK) {
            snprintf(key,sizeof(key),"vehicle-widget.layout.%d",at/CHUNK);
            if(!config_get(key,value,sizeof(value))){config_clear();return -1;}
            int count=n-at<CHUNK?n-at:CHUNK,i;
            for(i=0;i<count;++i) {
                int a=digit(value[i*2]),b=a<0?-1:digit(value[i*2+1]);
                if(a<0||b<0){config_clear();return -1;}bytes[at+i]=(unsigned char)((a<<4)|b);
            }
            if(value[i*2]){config_clear();return -1;}
        }
    } else n=read_file(LAYOUT_STOCK);
    config_clear();return n>0?write_ram(n):-1;
}
int layout_save(void) {
    int n=read_file(LAYOUT_RAM);if(n<0 || !config_load())return -1;
    char value[1024],key[80];const char *hex="0123456789abcdef";int ok=1;
    for(int at=0;at<n && ok;at+=CHUNK) {
        int count=n-at<CHUNK?n-at:CHUNK;
        for(int i=0;i<count;++i){value[i*2]=hex[bytes[at+i]>>4];value[i*2+1]=hex[bytes[at+i]&15];}value[count*2]=0;
        snprintf(key,sizeof(key),"vehicle-widget.layout.%d",at/CHUNK);ok=config_set(key,value);
    }
    snprintf(value,sizeof(value),"%d",n);ok=ok && config_set("vehicle-widget.layout.bytes",value) && config_commit();
    config_clear();return ok?0:-1;
}
void layout_cleanup(void){unlink(LAYOUT_RAM);unlink(LAYOUT_RAM ".next");}
