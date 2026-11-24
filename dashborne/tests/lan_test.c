#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#define open fake_open
#define read fake_read
#define close fake_close
#define socket fake_socket
#define ioctl fake_ioctl
#define bind fake_bind
#define sendto fake_sendto
#define recv fake_recv
#define __errno_location fake_errno
#include "../lan.c"
static const char *routes="Iface Destination Gateway Flags RefCnt Use Metric Mask\nwlan0 00000000 0101000A 0003 0 0 0 00000000\n";
static int offset,opened,closed,error=11,reply,fail,receives;
static unsigned char sent[28];
int fake_open(const char *p,int f,...) {assert(!strcmp(p,"/proc/net/route") && !f);offset=0;++opened;return 7;}
int fake_read(int f,void *b,unsigned int n) {assert(f==7);int left=(int)strlen(routes)-offset;if(left<1)return 0;if(n>(unsigned int)left)n=left;if(n>13)n=13;memcpy(b,routes+offset,n);offset+=n;return n;}
int fake_close(int f) {assert(f==7 || f==9);++closed;return 0;}
int fake_socket(int a,int b,int c) {assert(a==17 && b==(2|0x800|0x80000) && c==0x608);if(fail==1)return -1;++opened;return 9;}
int fake_ioctl(int f,unsigned long request,...) {
    assert(f==9);va_list v;va_start(v,request);unsigned char *r=va_arg(v,void *);va_end(v);
    assert(!strcmp((char *)r,"wlan0"));if(fail==2)return -1;
    if(request==0x8933){int index=4;memcpy(r+16,&index,4);}
    else {assert(request==0x8927);r[16]=1;memcpy(r+18,"\x02\x03\x04\x05\x06\x07",6);}
    return 0;
}
int fake_bind(int f,const void *p,unsigned int n) {const struct packet_address *a=p;assert(f==9 && n==20 && a->index==4);return fail==3?-1:0;}
int fake_sendto(int f,const void *p,unsigned int n,int flags,const void *addr,unsigned int len) {
    const struct packet_address *a=addr;assert(f==9 && n==28 && !flags && len==20 && a->mac[0]==255);
    memcpy(sent,p,28);assert(sent[7]==1 && !memcmp(sent+24,"\x0a\0\1\1",4));return fail==4?-1:28;
}
int fake_recv(int f,void *out,unsigned int n,int flags) {
    assert(f==9 && n>=28 && !flags);++receives;
    if(!reply)return -1;
    unsigned char p[28]={0,1,8,0,6,4,0,2};memcpy(p+8,"\x02\x10\x20\x30\x40\x50",6);
    memcpy(p+14,sent+24,4);memcpy(p+18,sent+8,6);memcpy(p+24,sent+14,4);
    if(reply==2)p[17]++;if(reply==3)p[27]++;if(reply==4)p[18]++;if(reply==5)p[7]=1;
    if(reply==6)p[8]=1;if(reply==7)memset(p+8,0,6);
    memcpy(out,p,28);return reply==8?10:28;
}
int *fake_errno(void) {return &error;}
static void reset(void) {lan_reset();assert(opened==closed);reply=fail=receives=0;error=11;}
int main(void) {
    for(int kind=0;kind<=8;++kind) {
        reset();assert(lan_poll("10.0.1.35")==0);reply=kind;
        if(kind==1){assert(lan_poll("10.0.1.35")==1);continue;}
        assert(lan_poll("10.0.1.35")==0);assert(lan_poll("10.0.1.35")==0);
        assert(lan_poll("10.0.1.35")==-1);assert(receives<=24);
    }
    for(int f=1;f<=4;++f){reset();fail=f;assert(lan_poll("10.0.1.35")==-1);}
    reset();assert(!lan_poll("10.0.1.35"));reply=1;assert(lan_poll("10.0.1.35")==1);
    assert(lan_poll("10.0.1.36")==0);assert(sent[17]==36);
    reply=0;error=5;assert(lan_poll("10.0.1.36")==-1);
    reset();assert(!lan_poll("10.0.1.35"));reply=1;assert(lan_poll("10.0.1.35")==1);
    reply=0;for(int i=0;i<7;++i)assert(lan_poll("10.0.1.35")==1);
    assert(lan_poll("10.0.1.35")==1); /* Refresh while last proof is still fresh. */
    for(int i=0;i<2;++i)assert(lan_poll("10.0.1.35")==1);assert(lan_poll("10.0.1.35")==-1);
    reset();routes="wlan0 00000000 0101000A 0001 0 0 0 00000000\n";assert(lan_poll("10.0.1.35")==-1);
    assert(lan_poll("10.0.1.999")==-1);reset();
    puts("PASS: fresh ARP replies, stale leases, wrong peers, bounded polling, expiration and socket failures");
}
