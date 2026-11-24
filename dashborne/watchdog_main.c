/* Standalone UI supervisor. A saved capture and configuration marker precede
   recovery. A Linux reboot releases display/driver allocations from the UI. */
typedef __SIZE_TYPE__ usize;
extern int open(const char *,int,...),close(int),mkdir(const char *,unsigned int),unlink(const char *);
extern int read(int,void *,unsigned int),write(int,const void *,unsigned int),fsync(int),flock(int,int);
extern long lseek(int,long,int);
extern int kill(int,int),fork(void),execv(const char *,char *const *),waitpid(int,int *,int);
struct instant {long sec,nsec;};
extern int clock_gettime(int,struct instant *),nanosleep(const struct instant *,struct instant *);
struct limits {unsigned long soft,hard;};
extern int setrlimit(int,const struct limits *);
extern void _exit(int);
extern void sync(void);
extern int watchdog_reboot(void);
#ifndef WD_ROOT
#define WD_ROOT "/tmp/dashborne-watchdog"
#define WD_CARD "/mnt/extsd/Dashborne"
#endif
#ifndef WD_PROC
#define WD_PROC "/proc"
#endif
#ifndef WD_TIMEOUT
#define WD_TIMEOUT 20
#endif
static unsigned int length(const char *s){unsigned int n=0;while(s[n])++n;return n;}
static int equal(const char *a,const char *b){while(*a && *a==*b){++a;++b;}return *a==*b;}
static int number(const char *s){int n=0;while(*s>='0' && *s<='9')n=n*10+*s++-'0';return n;}
static void decimal(char *out,unsigned long n) {
    char tmp[24];unsigned int at=0;do{tmp[at++]=(char)('0'+n%10);n/=10;}while(n);
    while(at)*out++=tmp[--at];*out=0;
}
static void join(char *out,const char *a,const char *b) {while(*a)*out++=*a++;while((*out++=*b++)){} }
static long now(void) {
    struct instant t={0};
#if defined(__APPLE__)
    clock_gettime(6,&t);
#else
    clock_gettime(1,&t);
#endif
    return t.sec;
}
static void pause_one(void){struct instant t={1,0};while(nanosleep(&t,&t)<0){} }
static int text(const char *path,char *out,unsigned int cap) {
    int fd=open(path,0);if(fd<0){*out=0;return 0;}
    int n=read(fd,out,cap-1);close(fd);if(n<0)n=0;out[n]=0;return n;
}
static int save(const char *path,const char *s) {
    int fd=open(path,1|0100|01000,0600);if(fd<0)return 0;
    unsigned int n=length(s);int ok=write(fd,s,n)==(int)n;fsync(fd);close(fd);return ok;
}
static void proc_path(char *out,const char *pid,const char *file) {
    char prefix[256];join(prefix,WD_PROC "/",pid);join(out,prefix,file);
}
static int identity(const char *pid,char *out) {
    char path[320],s[2048];proc_path(path,pid,"/stat");
    if(!text(path,s,sizeof(s)))return 0;
    char *p=s,*end=0;while(*p){if(*p==')')end=p;++p;}if(!end)return 0;
    p=end+1;int field=3;
    while(field<=22) {
        while(*p==' ')++p;if(!*p)return 0;
        char *start=p;while(*p && *p!=' ')++p;
        if(field++==22){unsigned int n=(unsigned int)(p-start);if(n>30)return 0;for(unsigned int i=0;i<n;++i)out[i]=start[i];out[n]=0;return 1;}
    }
    return 0;
}
static void copy_file(const char *from,const char *to,unsigned int cap,int tail,int nonblock) {
    int in=open(from,nonblock?04000:0);if(in<0)return;
    int out=open(to,1|0100|01000,0600);if(out<0){close(in);return;}
    if(tail){long n=lseek(in,0,2);(void)lseek(in,n>(long)cap?n-(long)cap:0,0);}
    char buf[4096];
    while(cap){unsigned int want=cap<sizeof(buf)?cap:sizeof(buf);int n=read(in,buf,want);if(n<=0)break;
        unsigned int done=0;while(done<(unsigned int)n){int w=write(out,buf+done,(unsigned int)n-done);if(w<=0)goto finish;done+=(unsigned int)w;}
        cap-=(unsigned int)n;}
finish:fsync(out);close(out);close(in);
}
static void capture(const char *pid,const char *reason,unsigned int slot) {
    char dir[256],suffix[32],path[320],source[320];decimal(suffix,slot);join(dir,WD_CARD "/diagnostics/watchdog-",suffix);
    (void)mkdir(WD_CARD "/diagnostics",0700);(void)mkdir(dir,0700);
    join(path,dir,"/reason.txt");save(path,reason);
    const char *files[]={"/status","/stat","/wchan","/stack","/maps"};
    for(unsigned int i=0;i<5;++i){proc_path(source,pid,files[i]);join(path,dir,files[i]);copy_file(source,path,65536,0,0);}
    join(path,dir,"/meminfo.txt");copy_file("/proc/meminfo",path,16384,0,0);
    join(path,dir,"/kernel.txt");copy_file("/dev/kmsg",path,262144,0,1);
    join(path,dir,"/loader.log");copy_file(WD_CARD "/loader.log",path,262144,1,0);
    join(path,dir,"/logcat.txt");
    (void)unlink(path); /* logcat -f appends; each capture needs a fresh file. */
    int child=fork();
    if(!child) {
        struct limits lim={524288,524288};setrlimit(1,&lim);
        char *args[]={"logcat","-t","2000","-f",path,(char *)0};execv("/bin/logcat",args);_exit(127);
    }
    if(child>0) {
        long deadline=now()+3;int status;
        while(waitpid(child,&status,1)==0){if(now()>=deadline){kill(child,9);waitpid(child,&status,0);break;}pause_one();}
    }
}
int main(int argc,char **argv) {
    if(argc!=2)return 2;int pid=number(argv[1]);if(pid<=1)return 2;
    char pid_text[32];decimal(pid_text,(unsigned int)pid);
    int lock=open(WD_ROOT "/lock",2|0100,0600);if(lock<0 || flock(lock,2|4))return 3;
    char count[32],owner[64],beat[96],last[96]={0},initial[32],current[32];
    text(WD_ROOT "/recoveries",count,sizeof(count));unsigned int recoveries=(unsigned int)number(count);
    if(recoveries>=3)return 4;
    int alive=identity(pid_text,initial);long changed=now();
    (void)save(WD_PROC "/self/oom_score_adj","-1000");
    /* A ready file lets the UI detect exec/locking failures before handoff. */
    if(!save(WD_ROOT "/ready",pid_text))return 5;
    for(;;) {
        text(WD_ROOT "/armed",owner,sizeof(owner));if(number(owner)!=pid)break;
        int same=alive && identity(pid_text,current) && equal(initial,current);
        text(WD_ROOT "/heartbeat",beat,sizeof(beat));
        if(number(beat)==pid && !equal(beat,last)) {join(last,beat,"");changed=now();}
        if(!same || now()-changed>=WD_TIMEOUT) {
            ++recoveries;decimal(count,recoveries);save(WD_ROOT "/recoveries",count);
            int boot_menu=save(WD_CARD "/config.once","");
            save(WD_ROOT "/recovery","1");
            save(WD_CARD "/recovery.once","1");
            capture(pid_text,same?"UI heartbeat stopped\n":"UI process exited\n",recoveries);
            /* The next boot stops in configuration, so recovery cannot form an
               automatic launch/crash loop. Flush the card before rebooting. */
            sync();
            if(boot_menu)(void)watchdog_reboot();
            /* Reboot failed: init can still recover this UI process. Recheck
               identity after capture; never signal a reused PID. */
            if(same && identity(pid_text,current) && equal(initial,current))kill(pid,9);
            break;
        }
        pause_one();
    }
    text(WD_ROOT "/armed",owner,sizeof(owner));
    if(number(owner)==pid){unlink(WD_ROOT "/armed");unlink(WD_ROOT "/ready");}
    close(lock);return 0;
}
