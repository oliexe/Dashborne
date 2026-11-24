#include <assert.h>
#include <math.h>
#include "../source.c"
static unsigned int q[64],m[64],s[64],v[160],paired_storage[8];
static unsigned short flags_storage;
static unsigned char demo_storage,b[64];
static int unit_mode,linked=1;
static int get_speed(void) {return 100;}
static int get_limit(void) {return 80;}
static int get_battery(void) {return 68;}
static int get_range(void) {return unit_mode?194:312;}
static int get_temp(void) {return unit_mode?70:21;}
static int unit(int id) {assert(id==0 || id==1 || id==8);return unit_mode;}
static int connection(const void *p) {assert(p==(const void *)paired);return linked;}
static int format(void *p) {assert(p==(void *)123);return 0;}
static void *clock_inst(void) {return (void *)123;}
static void *blinker_inst(void) {return b;}
static void fvalue(unsigned int offset,float n) {union {float f;unsigned int u;} value={.f=n};v[offset/4]=value.u;}
static unsigned char display[16];static int holds,releases;
static void *get_display(void) {return display;}
static void inhibit(void *d,int bit,int enabled) {
    assert(d==display && bit==0x80);
    if(enabled){display[8]|=bit;++holds;}else {display[8]&=~bit;++releases;}
}
int main(void) {
    display_instance=get_display;display_inhibit=inhibit;
    source_preview_awake(1);source_preview_awake(1);assert(holds==1 && !releases);
    source_preview_awake(0);source_preview_awake(0);assert(releases==1 && !display[8]);
    display[8]=0x80;source_preview_awake(1);source_preview_awake(0);
    assert(holds==1 && releases==1 && display[8]==0x80);
    quick=(void *)q;medium=(void *)m;slow=(void *)s;vehicle=(void *)v;flags=&flags_storage;demo=&demo_storage;
    paired=(void *)paired_storage;paired_storage[1]=12;
    speed=get_speed;limit=get_limit;battery=get_battery;range=get_range;temperature=get_temp;
    units=unit;connected_to=connection;clock12=format;clock_instance=clock_inst;blinkers=blinker_inst;
    m[0]=4;q[1]=40000;q[2]=(unsigned int)-1000;b[0x25]=b[0x35]='N';
    struct readings r;source_read(&r);assert(r.connected && r.speed==100 && r.limit==80);
    assert(!r.body_known && !r.body_open);
    assert(!r.blind_known && !r.blind_active && !r.blind_urgent);
    assert(r.power_valid && fabsf(r.power_kw-40)<0.01f && !r.nav_valid && !r.left && !r.right);
    unit_mode=1;source_read(&r);assert(r.speed==100 && r.limit==80 && r.mph && r.miles && r.fahrenheit);
    q[2]=500;source_read(&r);assert(r.power_kw==-20);
    m[0]=1;m[0x2c/4]=1;s[0x9c/4]=5480;s[0xa0/4]=377;
    source_read(&r);assert(r.charging && r.gear==1 && r.power_valid && fabsf(r.power_kw-11.009f)<0.02f);
    m[0]=4;m[0x2c/4]=0;
    b[0x25]='L';b[0x38]=1;source_read(&r);assert(r.left && !r.right);
    b[0x25]='N';
    v[0x44/4]=8;v[0x58/4]=v[0x60/4]=v[0x68/4]=1;
    fvalue(0x5c,20);fvalue(0x64,30);fvalue(0x6c,55);
    source_read(&r);assert(r.nav_valid && r.nav_distance==20 && r.nav_minutes==30 && r.nav_battery==55);
    unit_mode=0;source_read(&r);assert(fabsf(r.nav_distance-32.18688f)<0.001f);
    v[0x60/4]=0;source_read(&r);assert(!r.nav_valid);
    flags_storage=0x10;source_read(&r);assert(!r.connected);
    flags_storage=0;linked=0;source_read(&r);assert(!r.connected);
    linked=1;demo_storage=1;source_read(&r);assert(r.demo && !r.power_valid);
    demo_storage=0;paired_storage[1]=0;source_read(&r);assert(!r.connected);
    assert(!source_fingerprint("/does-not-exist",1,0));
}
