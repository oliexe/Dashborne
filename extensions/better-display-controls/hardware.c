#include "hardware.h"
extern int open(const char *,int,...),read(int,void *,unsigned int),close(int),ioctl(int,unsigned long,...);
extern unsigned long crc32(unsigned long,const unsigned char *,unsigned int);
extern void *dlopen(const char *,int);extern int dlclose(void *);
static void *library,*helper;
static int (*stock_brightness)(const void *);
static int captured,touched,base_min,base_black;
static float base_scale;
/* This helper layout is checked against the live 6.11.4 hardware library.
   No stock preference setters are called: only RAM calibration and /dev/disp. */
static int fingerprint(const char *path,unsigned int expected_size,unsigned long expected_crc) {
    int f=open(path,0);if(f<0)return 0;
    unsigned char b[4096];unsigned int size=0;unsigned long crc=0;int n;
    while((n=read(f,b,sizeof(b)))>0){size+=(unsigned int)n;if(size>expected_size){close(f);return 0;}crc=crc32(crc,b,(unsigned int)n);}
    close(f);return !n && size==expected_size && crc==expected_crc;
}
static int command(unsigned int op,int value) {
    int f=open("/dev/disp",2);if(f<0)return -1;
    unsigned long a[4]={0,(unsigned long)value,0,0};int result=ioctl(f,op,a);close(f);return result;
}
int display_hardware_bind(const struct dashborne_host *h) {
    if(!fingerprint("/lib/libzkhardware.so",38368,0xb83dc99b) ||
       !fingerprint("/lib/libeasyui.so",826052,0xece0feec))return -1;
    library=dlopen("/lib/libzkhardware.so",2);if(!library)return -1;
    void *(*instance)(void)=h->resolve(library,"_ZN16BrightnessHelper11getInstanceEv");
    stock_brightness=h->resolve(library,"_ZNK16BrightnessHelper13getBrightnessEv");
    if(!instance||!stock_brightness)return -1;
    helper=instance();if(!helper)return -1;
    int min=*(int *)helper,max=((int *)helper)[1];float scale=*(float *)((char *)helper+12);
    if(min<0||min>254||max!=100||scale<0.009f||scale>2.56f)return -1;
    int raw=command(0x103,0),black=command(0x191,0);
    return raw>=0&&raw<=255&&black>=0&&black<=100?0:-1;
}
int display_hardware_restore(void) {
    if(!touched)return 0;
    *(int *)helper=base_min;*(float *)((char *)helper+12)=base_scale;
    int ok=command(0x190,base_black)>=0 && command(0x191,0)==base_black;
    /* Keep the most recent stock requested level, and never wake a sleeping LCD. */
    int current=command(0x103,0);if(current<0)ok=0;
    if(current>0) {
        int value=stock_brightness(helper);if(value<0)value=0;if(value>100)value=100;
        int raw=base_min+(int)(value*base_scale);
        if(command(0x102,raw)<0||command(0x103,0)!=raw)ok=0;
    }
    if(ok)touched=captured=0;return ok?0:-1;
}
int display_hardware_apply(int brightness,int black) {
    if(!helper||brightness<1||brightness>255||black<0||black>100)return -1;
    if(!captured) {
        base_min=*(int *)helper;base_scale=*(float *)((char *)helper+12);base_black=command(0x191,0);
        if(base_black<0||base_black>100)return -1;captured=1;
    }
    touched=1;
    /* Stock updates and wake-up now map to this raw level, without periodic
       competing writes. Stock screenOff/backlightOff still write zero. */
    *(int *)helper=brightness;*(float *)((char *)helper+12)=0.0f;
    /* The native helper can report asleep while the live backlight is on
       after opening a custom Activity. Use the driver value as the authority. */
    int current=command(0x103,0),ok=current>=0;
    if(current>0 && current!=brightness)
        ok=command(0x102,brightness)>=0 && command(0x103,0)==brightness;
    if(command(0x191,0)!=black)ok=(command(0x190,black)>=0 && command(0x191,0)==black) && ok;
    if(!ok){display_hardware_restore();return -1;}return 0;
}
void display_hardware_close(void) {if(!touched&&library){dlclose(library);library=helper=0;}}
