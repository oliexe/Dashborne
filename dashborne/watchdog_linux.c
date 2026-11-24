/* Linux ARM EABI entry and syscalls: the supervisor needs no shared libraries. */
typedef __SIZE_TYPE__ usize;
void *memset(void *p,int value,usize n){unsigned char *s=p;while(n--)*s++=(unsigned char)value;return p;}
static long call(long nr,long a,long b,long c,long d) {
    register long r7 __asm__("r7")=nr,r0 __asm__("r0")=a,r1 __asm__("r1")=b,r2 __asm__("r2")=c,r3 __asm__("r3")=d;
    __asm__ volatile("svc 0":"+r"(r0):"r"(r7),"r"(r1),"r"(r2),"r"(r3):"memory","cc");return r0;
}
int open(const char *p,int flags,unsigned int mode){return (int)call(5,(long)p,flags,mode,0);}
int close(int fd){return (int)call(6,fd,0,0,0);}
int read(int fd,void *p,unsigned int n){return (int)call(3,fd,(long)p,n,0);}
int write(int fd,const void *p,unsigned int n){return (int)call(4,fd,(long)p,n,0);}
long lseek(int fd,long n,int origin){return call(19,fd,n,origin,0);}
int mkdir(const char *p,unsigned int mode){return (int)call(39,(long)p,mode,0,0);}
int unlink(const char *p){return (int)call(10,(long)p,0,0,0);}
int fsync(int fd){return (int)call(118,fd,0,0,0);}
int flock(int fd,int operation){return (int)call(143,fd,operation,0,0);}
int kill(int pid,int sig){return (int)call(37,pid,sig,0,0);}
int fork(void){return (int)call(2,0,0,0,0);}
int execv(const char *path,char *const *args){char *env[]={"PATH=/bin:/res/bin",(char *)0};return (int)call(11,(long)path,(long)args,(long)env,0);}
int waitpid(int pid,int *status,int options){return (int)call(114,pid,(long)status,options,0);}
int clock_gettime(int id,void *t){return (int)call(263,id,(long)t,0,0);}
int nanosleep(const void *t,void *remaining){return (int)call(162,(long)t,(long)remaining,0,0);}
int setrlimit(int resource,const void *limits){return (int)call(75,resource,(long)limits,0,0);}
void sync(void){(void)call(36,0,0,0,0);}
int watchdog_reboot(void){return (int)call(88,0xfee1dead,672274793,0x1234567,0);}
void _exit(int code){call(1,code,0,0,0);for(;;){} }
extern int main(int,char **);
__attribute__((used)) void watchdog_entry(unsigned long *stack){_exit(main((int)stack[0],(char **)(stack+1)));}
__attribute__((naked)) void _start(void){__asm__ volatile("mov r0, sp\nbl watchdog_entry");}
