/* Reversible ARM edits preserve the developer radio through native startup
   and projection requests. The controller retains an original-entry trampoline.
   No stock file is changed. */
#include "debug_force.h"
#include "core/widget-runtime/memory.h"
extern void *mmap(void *,__SIZE_TYPE__,int,int,int,long);
extern int mprotect(void *,__SIZE_TYPE__,int);
extern int munmap(void *,__SIZE_TYPE__);
extern long syscall(long,...);
static struct memory_edit edit, startup[3], daemon_guard;
static int installed;
static void (*original)(int), (*bypass)(int);

int debug_force_install(void (**change_mode)(int), void *network_init, void *maintenance) {
    if(!change_mode || !*change_mode || !network_init || !maintenance)return 0;
    if(installed) { *change_mode=bypass;return 1; }
    /* A failed protection/cache rollback must never count as an installed
       guard. Recover ownership before attempting another transaction. */
    if(edit.applied || startup[0].applied || startup[1].applied || startup[2].applied || daemon_guard.applied)
        if(!debug_force_remove(change_mode))return 0;
    unsigned int *maintain=maintenance;
    /* This native maintenance method only requests/retries ctl.stop=adbd.
       Vehicle data parsing and socket servicing are separate methods. */
    if(maintain[0]!=0xe92d4070 || maintain[1]!=0xe24dd028 ||
       maintain[2]!=0xe59f418c || maintain[3]!=0xe59f618c)return 0;
    unsigned int stop_before=0xe92d4070,stop_after=0xe12fff1e;
    if(memory_prepare(&daemon_guard,maintain,&stop_before,&stop_after,4))return 0;
    unsigned int *entry=(unsigned int *)*change_mode;
    /* Inspected net::change_mode ARM prologue, including the relocated load. */
    if(entry[0]!=0xe59f328c || entry[1]!=0xe59f228c ||
       entry[2]!=0xe08f3003 || entry[3]!=0xe92d40f0)return 0;
    /* Stock dashboard initialization tears down all three radio modes before
       requesting its hotspot. Preserve the already connected developer radio,
       but still run the remaining native initialization and listeners. */
    unsigned int *reset=(unsigned int *)network_init+0x10c/4;
    const unsigned int calls[]={0xebffe810,0xeb009f52,0xeb00bd9d};
    unsigned int nop=0xe320f000;
    for(unsigned int i=0;i<3;++i)
        if(memory_prepare(&startup[i],reset+i,&calls[i],&nop,4))return 0;
    if(!bypass) {
        unsigned int *code=mmap((void *)0,4096,3,0x22,-1,0);
        if(code==(void *)-1)return 0;
        code[0]=0xe59f3000; /* ldr r3, [pc] */
        code[1]=0xe59ff000; /* ldr pc, [pc] */
        code[2]=entry[0x294/4];code[3]=(unsigned int)(__UINTPTR_TYPE__)(entry+1);
        if(syscall(0xf0002L,code,code+4,0L) || mprotect(code,4096,5)){munmap(code,4096);return 0;}
        original=*change_mode;bypass=(void (*)(int))code;
    }
    if(*change_mode!=original)return 0;
    /* void change_mode returns immediately for stock callers. This one-word
       edit has no branch-range restriction and is atomic with other threads. */
    unsigned int branch=0xe12fff1e; /* bx lr */
    unsigned int expected=0xe59f328c;
    if(memory_prepare(&edit,entry,&expected,&branch,4) || memory_apply(&edit)) {
        (void)memory_restore(&edit);return 0;
    }
    for(unsigned int i=0;i<3;++i)if(memory_apply(&startup[i])) {
        (void)debug_force_remove(change_mode);return 0;
    }
    if(memory_apply(&daemon_guard)) {
        (void)debug_force_remove(change_mode);return 0;
    }
    installed=1;*change_mode=bypass;return 1;
}
int debug_force_remove(void (**change_mode)(int)) {
    int failed=memory_restore(&daemon_guard)!=0;
    for(unsigned int i=3;i>0;--i)if(memory_restore(&startup[i-1]))failed=1;
    if(memory_restore(&edit))failed=1;
    if(failed)return 0;
    installed=0;
    if(original && change_mode)*change_mode=original;
    /* Keep the tiny trampoline alive for any call already in flight. */
    return 1;
}
