/* Stable, temporary startup shim used only while replacing the SD installation. */
extern int fork(void),execl(const char *,const char *,...),waitpid(int,int *,int);
extern void _exit(int);
extern void *dlopen(const char *,int),*dlsym(void *,const char *);
static void (*init)(void *),(*deinit)(void *);
static const char *(*start)(void *);
#define EXPORT __attribute__((visibility("default")))
EXPORT void onEasyUIInit(void *ctx) {
    int child=fork(),status=-1;
    if(child==0){execl("/mnt/extsd/.dashborne-update/updater","updater","recover",(char *)0);_exit(127);}
    int ok=child>0 && waitpid(child,&status,0)==child && status==0;
    void *lib=dlopen(ok ? "/mnt/extsd/Dashborne/lib/libdashborne.so" : "/res/lib/libzkgui.so",2);
    if(!lib)lib=dlopen("/res/lib/libzkgui.so",2);
    if(lib){*(void **)(&init)=dlsym(lib,"onEasyUIInit");*(void **)(&deinit)=dlsym(lib,"onEasyUIDeinit");*(void **)(&start)=dlsym(lib,"onStartupApp");}
    if(init)init(ctx);
}
EXPORT void onEasyUIDeinit(void *ctx){if(deinit)deinit(ctx);}
EXPORT const char *onStartupApp(void *ctx){return start ? start(ctx) : (const char *)0;}
