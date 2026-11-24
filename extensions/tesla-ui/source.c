#include "source.h"
extern int open(const char *,int,...),read(int,void *,unsigned int),close(int);
extern unsigned long crc32(unsigned long,const unsigned char *,unsigned int);
extern int snprintf(char *,__SIZE_TYPE__,const char *,...);
struct timestamp {long sec,nsec;};
extern int clock_gettime(int,struct timestamp *);
extern long time(long *);
/* glibc struct tm on the target, also valid for the host test ABI. */
struct calendar {int sec,min,hour,mday,mon,year,wday,yday,isdst;long offset;const char *zone;};
extern struct calendar *localtime_r(const long *,struct calendar *);
static void *(*display_instance)(void);
static void (*display_inhibit)(void *,int,int);
static void *preview_display;
void source_preview_awake(int enabled) {
    if(!enabled) {
        if(preview_display)display_inhibit(preview_display,0x80,0);
        preview_display=0;return;
    }
    if(preview_display || !display_instance || !display_inhibit)return;
    void *d=display_instance();
    /* Same temporary inhibitor used by the dedicated projection viewer.
       Never clear a hold acquired elsewhere; manual display-off stays native. */
    if(d && !(((const unsigned char *)d)[8]&0x80)) {
        preview_display=d;display_inhibit(d,0x80,1);
    }
}
static int (*speed)(void),(*limit)(void),(*battery)(void),(*range)(void),(*temperature)(void);
static int (*units)(int),(*connected_to)(const void *),(*clock12)(void *);
static void *(*clock_instance)(void);
static void *(*blinkers)(void);
static const volatile unsigned char *quick,*medium,*slow,*vehicle,*demo,*paired;
static const volatile unsigned short *flags;
static int word(const volatile unsigned char *p,unsigned int offset) {
    return *(const volatile int *)(p+offset);
}
static float number(const volatile unsigned char *p,unsigned int offset) {
    union {unsigned int bits;float value;} n;
    n.bits=(unsigned int)word(p,offset);return n.value;
}
int source_fingerprint(const char *path,unsigned int size,unsigned int expected) {
    int fd=open(path,0);if(fd<0)return 0;
    unsigned char buffer[4096];unsigned int count=0;unsigned long crc=0;int n;
    while((n=read(fd,buffer,sizeof(buffer)))>0) {
        if((unsigned int)n>size-count) {close(fd);return 0;}
        count+=(unsigned int)n;crc=crc32(crc,buffer,(unsigned int)n);
    }
    close(fd);return n==0 && count==size && crc==expected;
}
int source_bind(const struct dashborne_host *h) {
    if(!source_fingerprint("/res/lib/libzkgui.so",3464120u,0x9de99542u) ||
       !source_fingerprint("/lib/libeasyui.so",826052u,0xece0feecu))return -1;
#define BIND(v,s) do {*(void **)(&(v))=h->resolve(h->official_library,s);if(!(v))return -1;}while(0)
    BIND(display_instance,"_ZN17DisplayController8instanceEv");
    BIND(display_inhibit,"_ZN17DisplayController17inhibitDisplayOffENS_11InhibitUserEb");
    BIND(speed,"_ZN14SideIndicators8getSpeedEv");
    BIND(limit,"_ZN14SideIndicators13getSpeedLimitEv");
    BIND(battery,"_ZN14SideIndicators20getBatteryPercentageEv");
    BIND(range,"_ZN14SideIndicators13getRangeValueEv");
    BIND(temperature,"_ZN14SideIndicators12getTempValueEv");
    BIND(units,"_ZN8WdgUnits12getUnitIndexE10UnitType_t");
    BIND(connected_to,"_ZN10InitSystem13isConnectedToERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE");
    BIND(clock12,"_ZN12ClockManager18getUse12HourFormatEv");
    BIND(clock_instance,"_ZN12ClockManager11getInstanceEv");
    BIND(blinkers,"_ZN8Blinkers8instanceEv");
    BIND(quick,"quickData");BIND(medium,"mediumData");BIND(slow,"slowData");BIND(vehicle,"vehicleData");
    BIND(flags,"_ZN10InitSystem13status_flags_E");
    BIND(demo,"_ZN13WdgDataSource16DEMO_MODE_ACTIVEE");
    BIND(paired,"_ZN10InitSystem21commander_paired_addrB5cxx11E");
#undef BIND
    return 0;
}
unsigned int source_seconds(void) {
    return (unsigned int)(source_microseconds()/1000000);
}
unsigned long long source_microseconds(void) {
    struct timestamp t={0};if(clock_gettime(1,&t))return 0;
    return (unsigned long long)t.sec*1000000+(unsigned long)t.nsec/1000;
}
void source_read(struct readings *r) {
    *r=(struct readings){0};
    /* Body-state mapping still requires ADB verification. Leave body_known=0;
       a zero-filled transport buffer must never imply six closed panels. */
    /* blind_known also stays zero until the stock blind-spot fields and their
       availability gate are verified. Blinkers are not occupancy sensors. */
    long now=time(0);struct calendar tm;
    if(now>1609459200 && localtime_r(&now,&tm)) {
        if(clock12(clock_instance()))snprintf(r->clock,sizeof(r->clock),"%d:%02d %s",tm.hour%12?tm.hour%12:12,tm.min,tm.hour>=12?"pm":"am");
        else snprintf(r->clock,sizeof(r->clock),"%02d:%02d",tm.hour,tm.min);
    }
    r->demo=*demo!=0;
    /* Match the stock disconnect gate; keep existing subscriptions untouched. */
    r->connected=word(paired,4)>0 && connected_to((const void *)paired) && !(*flags&0x30);
    r->mph=units(1)!=0;r->miles=units(0)!=0;r->fahrenheit=units(8)!=0;
    if(!r->connected || r->demo)return;
    /* These two getters already feed the stock gauges' displayed integers.
       Applying a second metric/imperial conversion would change their value. */
    r->speed=speed();r->limit=limit();
    r->battery=battery();r->range=range();r->temperature=temperature();
    r->gear=word(medium,0);r->charging=word(medium,0x2c)>0;
    /* Same battery voltage/current scaling used by the stock power widget.
       Its integer is tenths of kW; this view keeps signed kW. */
    int voltage=word(quick,4),current=word(quick,8);
    if(r->charging) {
        voltage=word(slow,0x9c);current=word(slow,0xa0);
        r->power_valid=voltage>0 && voltage<20000 && current>=0 && current<30000;
        r->power_kw=(float)voltage*0.073f*(float)current*0.073f/1000.0f;
    } else {
        r->power_valid=voltage>0 && voltage<100000 && current>-30000 && current<30000;
        r->power_kw=(float)voltage*0.01f*(float)current*-0.1f/1000.0f;
    }
    /* Navigation scalars only. Avoid sharing a mutable C++ destination string. */
    r->nav_valid=word(vehicle,0x44)>0 && vehicle[0x58] && vehicle[0x60] && vehicle[0x68];
    r->nav_distance=number(vehicle,0x5c)*(r->miles?1.0f:1.609344f);
    r->nav_minutes=number(vehicle,0x64);r->nav_battery=number(vehicle,0x6c);
    const volatile unsigned char *b=blinkers();
    r->left=b[0x25]!='N' && b[0x38];r->right=b[0x35]!='N' && b[0x38];
}
