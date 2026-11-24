#include <assert.h>
#include <string.h>
#include "../data.c"
static int linked=1;
static unsigned char b[64];
static int connected(const void *p){(void)p;return linked;}
static int get_speed(void){return 42;}
static void *get_blinkers(void){return b;}
int main(void) {
    unsigned int m[64]={0},s[64]={0},pair[8]={0};
    unsigned char d=0;unsigned short f=0;
    medium=(void *)m;slow=(void *)s;paired=(void *)pair;demo=&d;flags=&f;
    connected_to=connected;speed=get_speed;blinkers=get_blinkers;
    pair[1]=12;m[0]=TM_DRIVE;b[0x25]=b[0x35]='N';
    struct tm_reading r;tm_data_read(&r);assert(r.connected&&r.gear==TM_DRIVE&&r.speed==42);
    unsigned char *raw=(void *)s;const int offsets[]={0xd1,0xd3,0xd0,0xd2,0xd4,0xd5};
    for(unsigned int mask=0;mask<64;++mask) {
        for(int p=0;p<6;++p)raw[offsets[p]]=(mask>>p)&1;
        tm_data_read(&r);assert(r.known==63&&r.open==mask);
    }
    raw[0xd1]=255;tm_data_read(&r);assert(r.known==62&&r.open==62);
    b[0x25]='L';b[0x38]=1;tm_data_read(&r);assert(r.left&&!r.right);
    b[0x38]=0;tm_data_read(&r);assert(!r.left);
    f=0x10;tm_data_read(&r);assert(!r.connected&&!r.known);
    f=0;d=1;tm_data_read(&r);assert(!r.connected&&!r.known);
    d=0;linked=0;tm_data_read(&r);assert(!r.connected);
    linked=1;pair[1]=0;tm_data_read(&r);assert(!r.connected);
}
