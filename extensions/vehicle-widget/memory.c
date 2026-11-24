#include "memory.h"
extern int mprotect(void *,__SIZE_TYPE__,int);
extern long syscall(long,...);
extern void *fopen(const char *,const char *);
extern char *fgets(char *,int,void *);
extern int fclose(void *),sscanf(const char *,const char *,...);
static int same(const void *a,const void *b,unsigned int n) {
    const unsigned char *x=a,*y=b;while(n--)if(*x++!=*y++)return 0;return 1;
}
static int mapping(void *at,unsigned int n) {
    mem_address p=(mem_address)at,base=p&~(mem_address)4095;
    if(p+n< p || p+n>base+4096)return 0;
    void *f=fopen("/proc/self/maps","r");if(!f)return 0;
    char line[512],perms[8];unsigned long a,b;int mode=0;
    while(fgets(line,sizeof(line),f))if(sscanf(line,"%lx-%lx %4s",&a,&b,perms)==3 && base>=a && base+4096<=b) {
        if(perms[0]=='r' && perms[3]=='p')mode=1|(perms[1]=='w'?2:0)|(perms[2]=='x'?4:0);break;
    }
    fclose(f);return mode;
}
int memory_prepare(struct memory_edit *e,void *at,const void *before,const void *after,unsigned int n) {
    if(!e || !at || n!=4 || ((mem_address)at&3) || e->applied)return -1;
    int mode=mapping(at,n);if(!mode || !same(at,before,n))return -1;
    *e=(struct memory_edit){.at=at,.size=n,.protection=mode};
    for(unsigned int i=0;i<n;++i){e->before[i]=((const unsigned char *)before)[i];e->after[i]=((const unsigned char *)after)[i];}return 0;
}
static int replace(struct memory_edit *e,int restore) {
    if(!e->at)return -1;
    const unsigned char *value=restore?e->before:e->after,*expected=restore?e->after:e->before;
    if(!same(e->at,expected,e->size) && !same(e->at,value,e->size))return -1;
    void *page=(void *)((mem_address)e->at&~(mem_address)4095);
    if(mprotect(page,4096,e->protection|2))return -1;
    /* Keep ownership on any cache/protection failure so unload is refused. */
    e->applied=1;
    if(e->size==4 && !((mem_address)e->at&3)) {
        unsigned int word=0;for(unsigned int i=0;i<4;++i)word|=(unsigned int)value[i]<<(i*8);
        *(volatile unsigned int *)e->at=word;
    } else return -1; /* Runtime changes must be aligned atomic words. */
    int failed=0;
    if(e->protection&4)failed=syscall(0xf0002L,(mem_address)e->at,(mem_address)e->at+e->size,0L)!=0;
    if(mprotect(page,4096,e->protection))failed=1;
    if(!failed && restore)e->applied=0;
    return failed?-1:0;
}
int memory_apply(struct memory_edit *e){return replace(e,0);}
int memory_restore(struct memory_edit *e){return !e->applied?0:replace(e,1);}
