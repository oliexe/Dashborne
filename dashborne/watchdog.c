/* Parent side: heartbeat comes exclusively from the UI event loop. */
#include "watchdog.h"
extern int open(const char *,int,...),read(int,void *,unsigned int),write(int,const void *,unsigned int);
extern int close(int),mkdir(const char *,unsigned int),rename(const char *,const char *),unlink(const char *);
extern int getpid(void),fork(void),execv(const char *,char *const *),kill(int,int),fsync(int);
extern int waitpid(int,int *,int),setsid(void);
extern int snprintf(char *,__SIZE_TYPE__,const char *,...);
extern void _exit(int);
#define ROOT "/tmp/dashborne-watchdog"
static int armed, child;
static unsigned int sequence;
static int owner(void) {
    char s[32]={0};int fd=open(ROOT "/armed",0),pid=0;
    if(fd<0)return 0;int n=read(fd,s,sizeof(s)-1);close(fd);
    for(int i=0;i<n && s[i]>='0' && s[i]<='9';++i)pid=pid*10+s[i]-'0';
    return pid;
}
static int put(const char *path,const char *s,unsigned int n) {
    int fd=open(path,1|0100|01000,0600);if(fd<0)return 0;
    int ok=write(fd,s,n)==(int)n;close(fd);return ok;
}
int watchdog_ready(void) {
    if(!armed)return 0;
    char s[32]={0},expected[32];int fd=open(ROOT "/ready",0);if(fd<0)return 0;
    int n=read(fd,s,sizeof(s)-1);close(fd);
    int len=snprintf(expected,sizeof(expected),"%d",getpid());if(n!=len)return 0;
    for(int i=0;i<len;++i)if(s[i]!=expected[i])return 0;
    int status;return child>0 && waitpid(child,&status,1)==0;
}
void watchdog_beat(void) {
    if(!armed)return;
    char s[64];int n=snprintf(s,sizeof(s),"%d %u\n",getpid(),++sequence);
    if(put(ROOT "/heartbeat.next",s,(unsigned int)n))
        (void)rename(ROOT "/heartbeat.next",ROOT "/heartbeat");
}
int watchdog_start(void) {
    if(armed)return 1;
    /* A separate executable has no inherited UI/library locks or heap. */
    int fd=open("/mnt/extsd/Dashborne/bin/watchdog",0);if(fd<0)return 0;close(fd);
    (void)mkdir(ROOT,0700);
    (void)unlink(ROOT "/ready");
    char pid[32];int n=snprintf(pid,sizeof(pid),"%d\n",getpid());
    if(!put(ROOT "/armed.next",pid,(unsigned int)n) || rename(ROOT "/armed.next",ROOT "/armed"))return 0;
    armed=1;watchdog_beat();
    child=fork();
    if(child<0){watchdog_stop();return 0;}
    if(!child) {
        (void)setsid();
        for(int i=3;i<1024;++i)close(i);
        char *args[]={"watchdog",pid,(char *)0};
        execv("/mnt/extsd/Dashborne/bin/watchdog",args);_exit(127);
    }
    return 1;
}
void watchdog_stop(void) {
    if(armed && owner()==getpid()) {
        (void)unlink(ROOT "/armed");(void)unlink(ROOT "/heartbeat");
    }
    armed=0;
    if(child>0){int status;if(waitpid(child,&status,1)==child)child=0;}
}
int watchdog_recovery(void) {
    int recovered=0;
    const char *markers[]={ROOT "/recovery","/mnt/extsd/Dashborne/recovery.once"};
    for(unsigned int i=0;i<2;++i) {
        int fd=open(markers[i],0);
        if(fd>=0){close(fd);(void)unlink(markers[i]);recovered=1;}
    }
    if(recovered)return 1;
    int pid=owner();
    return pid>0 && pid!=getpid();
}
