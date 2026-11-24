#include "lan.h"
extern int open(const char *,int,...),read(int,void *,unsigned int),close(int);
extern int socket(int,int,int),ioctl(int,unsigned long,...);
extern int bind(int,const void *,unsigned int);
extern int sendto(int,const void *,unsigned int,int,const void *,unsigned int);
extern int recv(int,void *,unsigned int,int);
extern int *__errno_location(void);
extern int sscanf(const char *,const char *,...);
extern int strcmp(const char *,const char *);
extern void *memcpy(void *,const void *,__SIZE_TYPE__);
extern int memcmp(const void *,const void *,__SIZE_TYPE__);
struct packet_address {unsigned short family,protocol;int index;unsigned short type;unsigned char packet_type,length,mac[8];};
_Static_assert(sizeof(struct packet_address)==20,"Linux sockaddr_ll");
static int fd=-1,age,waiting,verified;
static const char *failure="LAN check failed";
const char *lan_error(void) {return failure;}
static unsigned char local[4],gateway[4],mac[6];
static struct packet_address peer;
void lan_reset(void) {if(fd>=0)close(fd);fd=-1;age=waiting=verified=0;}
static int route(unsigned char out[4]) {
    failure="No Wi-Fi default route";
    int f=open("/proc/net/route",0);if(f<0)return 0;
    char buffer[4096];int used=0,n;
    while(used<(int)sizeof(buffer)-1 && (n=read(f,buffer+used,sizeof(buffer)-1-used))>0)used+=n;
    close(f);buffer[used]=0;
    for(char *line=buffer;*line;) {
        char name[16];unsigned int dest,gw,flags,mask,metric;
        if(sscanf(line,"%15s %x %x %x %*u %*u %u %x",name,&dest,&gw,&flags,&metric,&mask)==6 &&
           !strcmp(name,"wlan0") && !dest && !mask && (flags&3)==3 && gw) {
            for(int i=0;i<4;++i)out[i]=(unsigned char)(gw>>(8*i));
            return out[0] && out[0]!=127 && out[0]<224;
        }
        while(*line && *line!='\n')++line;
        if(*line)++line;
    }
    return 0;
}
static int ip_bytes(const char *ip,unsigned char out[4]) {
    unsigned int a,b,c,d;char tail;
    if(sscanf(ip,"%u.%u.%u.%u%c",&a,&b,&c,&d,&tail)!=4 || a>255 || b>255 || c>255 || d>255)return 0;
    out[0]=a;out[1]=b;out[2]=c;out[3]=d;return 1;
}
static int start(const unsigned char ip[4],const unsigned char gw[4]) {
    union {unsigned long align;unsigned char bytes[40];} r={0};
    /* AF_PACKET datagram adds the Ethernet header; ARP itself is 28 bytes.
       NONBLOCK and CLOEXEC keep the UI and child processes independent. */
    failure="Cannot open LAN probe socket";
    fd=socket(17,2|0x800|0x80000,0x0608);if(fd<0)return -1;
    memcpy(r.bytes,"wlan0",6);
    failure="Cannot read Wi-Fi interface index";
    if(ioctl(fd,0x8933,&r)<0)goto bad;
    peer=(struct packet_address){.family=17,.protocol=0x0608,.length=6};
    memcpy(&peer.index,r.bytes+16,4);
    failure="Cannot bind LAN probe to Wi-Fi";
    if(peer.index<=0 || bind(fd,&peer,sizeof(peer))<0)goto bad;
    r=(__typeof__(r)){0};memcpy(r.bytes,"wlan0",6);
    failure="Cannot read Wi-Fi hardware address";
    if(ioctl(fd,0x8927,&r)<0 || r.bytes[16]!=1 || r.bytes[17])goto bad;
    memcpy(mac,r.bytes+18,6);if((mac[0]&1) || !memcmp(mac,"\0\0\0\0\0\0",6))goto bad;
    memcpy(local,ip,4);memcpy(gateway,gw,4);
    for(int i=0;i<6;++i)peer.mac[i]=255;
    unsigned char packet[28]={0,1,8,0,6,4,0,1};
    memcpy(packet+8,mac,6);memcpy(packet+14,local,4);memcpy(packet+24,gateway,4);
    failure="Cannot send LAN probe";
    if(sendto(fd,packet,sizeof(packet),0,&peer,sizeof(peer))!=sizeof(packet))goto bad;
    waiting=1;age=0;return 0;
bad:
    lan_reset();return -1;
}
int lan_poll(const char *ip) {
    unsigned char current[4],gw[4];
    if(!ip_bytes(ip,current) || !route(gw)){lan_reset();return -1;}
    if(fd>=0 && (memcmp(current,local,4) || memcmp(gw,gateway,4)))lan_reset();
    if(fd<0)return start(current,gw);
    if(!waiting) {
        if(++age<8)return 1;
        lan_reset();
        int result=start(current,gw);
        if(result<0)return result;
        verified=1;return 1;
    }
    for(int i=0;i<8;++i) {
        unsigned char packet[64];int n=recv(fd,packet,sizeof(packet),0);
        if(n<0) {
            int e=*__errno_location();if(e!=11 && e!=4){failure="LAN probe receive failed";lan_reset();return -1;}
            break;
        }
        const unsigned char header[]={0,1,8,0,6,4,0,2};
        if(n>=28 && !memcmp(packet,header,8) && !(packet[8]&1) &&
           memcmp(packet+8,"\0\0\0\0\0\0",6) && !memcmp(packet+14,gateway,4) &&
           !memcmp(packet+18,mac,6) && !memcmp(packet+24,local,4)) {
            waiting=0;verified=1;age=0;return 1;
        }
    }
    if(++age>=3){failure="Router did not answer LAN probe";lan_reset();return -1;}
    return verified?1:0;
}
