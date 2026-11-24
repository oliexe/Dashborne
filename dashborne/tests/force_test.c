/* Verify reversible radio edits and rollback without executing ARM on the host. */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#define mmap test_map
#define mprotect test_protect
#define munmap test_unmap
#define syscall test_syscall
#include "../debug_force.c"
static unsigned int trampoline[1024];
static int fail_map,fail_protect,fail_flush,fail_apply,applies;
void *test_map(void *p,__SIZE_TYPE__ n,int prot,int flags,int fd,long off){
    (void)p;(void)n;(void)prot;(void)flags;(void)fd;(void)off;
    return fail_map?(void *)-1:trampoline;
}
int test_protect(void *p,__SIZE_TYPE__ n,int prot){(void)p;(void)n;(void)prot;return fail_protect;}
int test_unmap(void *p,__SIZE_TYPE__ n){(void)p;(void)n;return 0;}
long test_syscall(long nr,...){(void)nr;return fail_flush;}
int memory_prepare(struct memory_edit *e,void *p,const void *before,const void *after,unsigned int n){
    if(e->applied || memcmp(p,before,n))return -1;
    e->at=p;e->size=n;memcpy(e->before,before,n);memcpy(e->after,after,n);return 0;
}
int memory_apply(struct memory_edit *e){
    e->applied=1;memcpy(e->at,e->after,e->size);
    return ++applies==fail_apply?-1:0;
}
int memory_restore(struct memory_edit *e){
    if(e->applied){memcpy(e->at,e->before,e->size);e->applied=0;}return 0;
}
int main(void){
    unsigned int mode[170]={0xe59f328c,0xe59f228c,0xe08f3003,0xe92d40f0};
    unsigned int init[80]={0};
    unsigned int maintain[]={0xe92d4070,0xe24dd028,0xe59f418c,0xe59f618c};
    const unsigned int calls[]={0xebffe810,0xeb009f52,0xeb00bd9d};
    memcpy(init+0x10c/4,calls,sizeof(calls));mode[0x294/4]=0x12345678;
    void (*fn)(int)=(void (*)(int))mode;
    assert(!debug_force_install(&fn,NULL,maintain));
    init[0x110/4]^=1;assert(!debug_force_install(&fn,init,maintain));init[0x110/4]^=1;
    fail_map=1;assert(!debug_force_install(&fn,init,maintain));fail_map=0;
    fail_protect=1;assert(!debug_force_install(&fn,init,maintain));fail_protect=0;
    fail_flush=1;assert(!debug_force_install(&fn,init,maintain));fail_flush=0;
    for(int failure=1;failure<=5;++failure){
        applies=0;fail_apply=failure;assert(!debug_force_install(&fn,init,maintain));
        assert(mode[0]==0xe59f328c && !memcmp(init+0x10c/4,calls,sizeof(calls)));
        assert(fn==(void (*)(int))mode);assert(maintain[0]==0xe92d4070);
    }
    applies=fail_apply=0;assert(debug_force_install(&fn,init,maintain));
    assert(mode[0]==0xe12fff1e && fn==(void (*)(int))trampoline);
    assert(trampoline[2]==0x12345678);
    for(int i=0;i<3;++i)assert(init[0x10c/4+i]==0xe320f000);
    assert(debug_force_install(&fn,init,maintain));assert(applies==5);
    assert(debug_force_remove(&fn));assert(debug_force_remove(&fn));
    assert(mode[0]==0xe59f328c && !memcmp(init+0x10c/4,calls,sizeof(calls)));
    assert(fn==(void (*)(int))mode);assert(maintain[0]==0xe92d4070);
    assert(debug_force_install(&fn,init,maintain));assert(debug_force_remove(&fn));
    return 0;
}
