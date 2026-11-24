#include <assert.h>
#include <stdio.h>
#include <string.h>
#define fopen mock_open
#define fgets mock_gets
#define fclose mock_close
#define mprotect mock_protect
#define syscall mock_flush
#include "../memory.c"
#undef fopen
#undef fgets
#undef fclose
#undef mprotect
#undef syscall
static _Alignas(4096) unsigned char region[8192];
static int protect_fail,flush_fail;
void *mock_open(const char *p,const char *m){assert(!strcmp(p,"/proc/self/maps")&&!strcmp(m,"r"));return (void *)1;}
char *mock_gets(char *s,int n,void *f){(void)f;snprintf(s,(size_t)n,"%lx-%lx r-xp",(unsigned long)region,(unsigned long)region+sizeof(region));return s;}
int mock_close(void *f){(void)f;return 0;}
int mock_protect(void *p,__SIZE_TYPE__ n,int mode){assert((p==region||p==region+4096)&&n==4096&&(mode==3||mode==5||mode==7));return protect_fail?-1:0;}
long mock_flush(long call,...){assert(call==0xf0002L);return flush_fail?-1:0;}
int main(void) {
    struct memory_edit e={0};unsigned int old=0xe59f30b0,new=7,*at=(void *)region;
    *at=old;assert(!memory_prepare(&e,at,&old,&new,4));assert(!memory_apply(&e)&&e.applied&&*at==new);
    *at=9;assert(memory_restore(&e)==-1&&e.applied);*at=new;
    protect_fail=1;assert(memory_restore(&e)==-1&&e.applied);protect_fail=0;
    flush_fail=1;assert(memory_restore(&e)==-1&&e.applied&&*at==old);flush_fail=0;
    assert(!memory_restore(&e)&&!e.applied&&*at==old);
    assert(memory_prepare(&e,region+1,&old,&new,4)==-1);
    assert(memory_prepare(&e,region,&old,&new,8)==-1);
    e=(struct memory_edit){0};*at=old;
    assert(!memory_prepare(&e,at,&old,&new,4));flush_fail=1;
    assert(memory_apply(&e)==-1 && e.applied && *at==new);
    flush_fail=0;assert(!memory_restore(&e)&&*at==old);
    puts("Widget memory hooks: atomic words, conflict and failure recovery passed");
}
