/* SD launcher probe. No shell commands, network, flash writes or USB changes.
 * The sole DT_NEEDED dependency is the device's existing /res/lib/libzkgui.so.
 * dlsym on this library can therefore find the original FlyThings entry points
 * through that dependency without us assuming their C/C++ calling signatures.
 * All writes are confined to the mounted test card. */
typedef unsigned int usize;
extern int open(const char *, int, ...);
extern int read(int, void *, usize);
extern int write(int, const void *, usize);
extern int close(int);
extern int fsync(int);
extern int mkdir(const char *, unsigned int);

#define ROOT "/mnt/extsd/"
#define RESULT ROOT "DASH_TEST_RESULTS/"
#define RDONLY 0
#define WRONLY 1
#define CREAT 0100
#define TRUNC 01000
#define APPEND 02000
#define NOFOLLOW 0400000

static usize slen(const char *s) { usize n=0; while (s[n]) ++n; return n; }
static int putall(int fd, const char *p, usize n) {
    while (n) { int k=write(fd,p,n); if (k<=0) return -1; p+=k; n-=(usize)k; }
    return 0;
}
static void note(const char *s) {
    int fd=open(RESULT "probe.log", WRONLY|CREAT|APPEND|NOFOLLOW,0600);
    if (fd<0) return;
    putall(fd,s,slen(s)); fsync(fd); close(fd);
}
static void copy_readonly(const char *src,const char *dst) {
    char buf[4096]; unsigned int total=0; int count;
    int in=open(src,RDONLY); if(in<0) {note("Unavailable: ");note(src);note("\n");return;}
    int out=open(dst,WRONLY|CREAT|TRUNC|NOFOLLOW,0600);
    if(out<0) {close(in);return;}
    while(total<8*1024*1024 && (count=read(in,buf,sizeof(buf)))>0) {
        if(putall(out,buf,(usize)count)<0) break;
        total+=(unsigned int)count;
    }
    fsync(out);close(out);close(in);
    note("Read: ");note(src);note("\n");
}
__attribute__((constructor)) static void sd_probe_loaded(void) {
    /* Require our card token before creating any output. */
    char token[16];
    int gate=open(ROOT "DASH_SD_PROBE_V1",RDONLY|NOFOLLOW);
    if(gate<0) return;
    int n=read(gate,token,sizeof(token));close(gate);
    if(n<12) return;
    const char expected[]="DASH_SD_TEST";
    for(int i=0;i<12;++i) if(token[i]!=expected[i]) return;
    mkdir(RESULT,0700);
    int fd=open(RESULT "SD_CODE_EXECUTED.txt",WRONLY|CREAT|TRUNC|NOFOLLOW,0600);
    if(fd>=0) {
        const char msg[]="SUCCESS: the DASH Linux loader executed libsdprobe.so from microSD.\n"
            "The installed /res/lib/libzkgui.so is the application dependency.\n"
            "This proves library execution, not that every UI callback completed.\n";
        putall(fd,msg,sizeof(msg)-1);fsync(fd);close(fd);
    }
    note("SD probe constructor entered.\n");
    copy_readonly("/proc/version",RESULT "kernel-version.txt");
    copy_readonly("/proc/mounts",RESULT "mounts.txt");
    copy_readonly("/proc/self/maps",RESULT "loader-maps.txt");
    copy_readonly("/etc/init.rc",RESULT "etc-init.rc");
    copy_readonly("/init.rc",RESULT "root-init.rc");
    copy_readonly("/res/etc/EasyUI.cfg",RESULT "installed-EasyUI.cfg");
    copy_readonly("/bin/zkgui",RESULT "zkgui.bin");
    copy_readonly("/bin/zkdaemon",RESULT "zkdaemon.bin");
    copy_readonly("/lib/libeasyui.so",RESULT "libeasyui.so");
    copy_readonly("/lib/libzkupgrade.so",RESULT "libzkupgrade.so");
    note("Probe complete; original application entry points remain available.\n");
}
