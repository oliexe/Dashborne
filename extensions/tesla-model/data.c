#include "stock_compat.h"
#include "data.h"
extern int open(const char *,int,...),read(int,void *,unsigned int),close(int);
extern unsigned long crc32(unsigned long,const unsigned char *,unsigned int);
static const volatile unsigned char *medium,*slow,*paired,*demo;
static const volatile unsigned short *flags;
static int (*connected_to)(const void *),(*speed)(void);
static void *(*blinkers)(void);
static int word(const volatile unsigned char *p,unsigned int offset) {
    return *(const volatile int *)(p+offset);
}
static int fingerprint(const char *path,unsigned int size,unsigned int expected) {
    int fd=open(path,0);if(fd<0)return 0;
    unsigned char bytes[4096];unsigned int total=0;unsigned long crc=0;int n;
    while((n=read(fd,bytes,sizeof(bytes)))>0) {
        if((unsigned int)n>size-total){close(fd);return 0;}
        total+=(unsigned int)n;crc=crc32(crc,bytes,(unsigned int)n);
    }
    close(fd);return n==0 && total==size && crc==expected;
}
int tm_data_bind(const struct dashborne_host *h) {
    if(!db_stock_version() ||
       !fingerprint("/lib/libeasyui.so",826052,0xece0feec)) {
        h->log(h,"tesla-model: unsupported app/runtime; vehicle offsets were not read");return -1;
    }
#define BIND(v,s) do {*(void **)(&(v))=h->resolve(h->official_library,s);if(!(v)){h->log(h,"tesla-model: missing " s);return -1;}}while(0)
    BIND(medium,"mediumData");BIND(slow,"slowData");
    BIND(paired,"_ZN10InitSystem21commander_paired_addrB5cxx11E");
    BIND(flags,"_ZN10InitSystem13status_flags_E");
    BIND(demo,"_ZN13WdgDataSource16DEMO_MODE_ACTIVEE");
    BIND(connected_to,"_ZN10InitSystem13isConnectedToERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE");
    BIND(speed,"_ZN14SideIndicators8getSpeedEv");BIND(blinkers,"_ZN8Blinkers8instanceEv");
#undef BIND
    return 0;
}
void tm_decode_panels(struct tm_reading *r,const volatile unsigned char *data) {
    r->known=r->open=0;if(!data || !r->connected)return;
    const unsigned int offsets[]={0xd1,0xd3,0xd0,0xd2,0xd4,0xd5};
    for(unsigned int p=0;p<TM_PANEL_COUNT;++p) {
        unsigned int value=data[offsets[p]];
        if(value<=1){r->known|=1u<<p;if(value)r->open|=1u<<p;}
    }
}
void tm_data_read(struct tm_reading *r) {
    *r=(struct tm_reading){0};
    r->connected=!*demo && word(paired,4)>0 && connected_to((const void *)paired) && !(*flags&0x30);
    if(!r->connected)return;
    r->gear=word(medium,0);r->speed=speed();tm_decode_panels(r,slow);
    const volatile unsigned char *b=blinkers();
    /* The stock output is G/R for an illuminated turn signal, O for a
       side warning without a turn signal, and N for off. O is not a lamp. */
    if(b){r->left=(b[0x25]=='G' || b[0x25]=='R') && b[0x38];
          r->right=(b[0x35]=='G' || b[0x35]=='R') && b[0x38];}
    /* blinkersCalculate reads these separate side-detection states. The stock
       app treats 0/3 as clear; only the defined 1/2 warning states are shown. */
    int left=word(medium,0x34),right=word(medium,0x38);
    r->detected_left=left==1 || left==2;r->detected_right=right==1 || right==2;
    /* Verified through ADB: getAct_Low_Beam_38 and getAct_High_Beam_37
       read mediumData bytes 0x5e/0x5f in the fingerprinted app. */
    unsigned int low=medium[0x5e],high=medium[0x5f];
    if(low<=1 && high<=1){r->lights_known=1;r->low_beam=low;r->high_beam=high;}
}
