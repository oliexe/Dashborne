/* Out-of-process downloads keep the native UI responsive and memory bounded. */
#include "update.h"
extern int fork(void),setsid(void),execl(const char *,const char *,...),waitpid(int,int *,int);
extern int open(const char *,int,...),read(int,void *,unsigned int),close(int),unlink(const char *),access(const char *,int);
extern void _exit(int);
extern int *__errno_location(void);
static int child;
static char message[256]="Connect to Wi-Fi, then check for releases";
static struct update_release releases[100];
static unsigned int count;
static int fetching;
static void copy(char *dst,const char *src,unsigned int cap){unsigned int i=0;while(src[i] && i+1<cap){dst[i]=src[i];++i;}dst[i]=0;}
static int equal(const char *a,const char *b){while(*a && *a==*b){++a;++b;}return *a==*b;}
static int field(char **p,char *out,unsigned int cap){unsigned int n=0;while(**p && **p!='\t' && **p!='\n'){if(n+1>=cap)return 0;out[n++]=*(*p)++;}out[n]=0;if(**p=='\t'){++*p;return 1;}return 0;}
static void load_list(void){
    static char buf[56000];int fd=open("/tmp/dashborne-releases.tsv",0);if(fd<0)return;
    unsigned int total=0;int n;while(total+1<sizeof(buf) && (n=read(fd,buf+total,sizeof(buf)-total-1))>0)total+=(unsigned int)n;
    close(fd);buf[total]=0;count=0;char *p=buf;
    while(*p && count<100){struct update_release r={0};char supported[8];
        if(!field(&p,r.id,sizeof(r.id)) || !field(&p,r.version,sizeof(r.version)) || !field(&p,r.os,sizeof(r.os)) || !field(&p,supported,sizeof(supported)))break;
        unsigned int i=0;while(*p && *p!='\n'){if(i+1<sizeof(r.notes))r.notes[i++]=*p;++p;}r.notes[i]=0;if(*p)++p;
        r.supported=equal(supported,"true");releases[count++]=r;
    }
}
int update_start(const char *action,const char *id){
    if(child)return 0;
    fetching=equal(action,"list");if(fetching){count=0;(void)unlink("/tmp/dashborne-releases.tsv");}
    (void)unlink("/tmp/dashborne-update.status");
    int pid=fork();if(pid<0){copy(message,"Could not start updater",sizeof(message));return 0;}
    if(!pid){(void)setsid();execl("/mnt/extsd/Dashborne/bin/updater","updater",action,id,(char *)0);_exit(127);}
    child=pid;copy(message,"Working...",sizeof(message));return 1;
}
void update_poll(void){
    if(!child)return;
    int fd=open("/tmp/dashborne-update.status",0);if(fd>=0){int n=read(fd,message,sizeof(message)-1);if(n>0)message[n]=0;close(fd);}
    int status=0,result=waitpid(child,&status,1);
    if(result==child || (result<0 && *__errno_location()==10)){child=0;if(status && equal(message,"Working..."))copy(message,"Updater could not start",sizeof(message));if(!status && fetching)load_list();else if(!status && equal(message,"Working..."))copy(message,"Connect to Wi-Fi, then check for releases",sizeof(message));fetching=0;}
}
int update_busy(void){return child!=0 || access("/mnt/extsd/.dashborne-update/journal.json",0)==0;}
const char *update_status(void){return message;}
unsigned int update_count(void){return count;}
const struct update_release *update_release(unsigned int index){return index<count ? &releases[index] : (const struct update_release *)0;}
void update_confirm(void){if(access("/mnt/extsd/.dashborne-update/journal.json",0)==0)(void)update_start("confirm",(const char *)0);}
