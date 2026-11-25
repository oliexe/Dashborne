#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define open fake_open
#define close fake_close
#define read fake_read
#define crc32 fake_crc32
#define fopen fake_fopen
#define fgets fake_fgets
#define fclose fake_fclose
#define dladdr fake_dladdr
#include "../extension.c"
#undef open
#undef close
#undef read
#undef crc32
#undef fopen
#undef fgets
#undef fclose
#undef dladdr
_Alignas(unsigned int) static unsigned char memory[0x359100];
static int modern;
static void *image_base(void){return memory+(modern?4:0);}
static int remaining,lines,unsupported,readonly,missing,wrong_symbol,read_error;
int fake_open(const char *p,int flags,...) {(void)p;(void)flags;remaining=3464120;return 1;}
int fake_close(int fd) {(void)fd;return 0;}
int fake_read(int fd,void *p,unsigned int size) {
    (void)fd;(void)p;if(read_error)return -1;
    int n=remaining<(int)size?remaining:(int)size;remaining-=n;return n;
}
unsigned long fake_crc32(unsigned long crc,const unsigned char *p,unsigned int n) {
    (void)crc;(void)p;(void)n;return unsupported?0:modern?0xed0e1aa4u:0x9de99542u;
}
void *fake_fopen(const char *p,const char *mode) {(void)p;(void)mode;lines=0;return memory;}
char *fake_fgets(char *line,int n,void *file) {
    (void)file;if(lines++)return 0;
    snprintf(line,(size_t)n,"%lx-%lx %s 00000000 00:00 0\n",
        (unsigned long)memory,(unsigned long)(memory+sizeof(memory)),readonly?"r--p":"rw-p");return line;
}
int fake_fclose(void *f) {(void)f;return 0;}
int fake_dladdr(const void *p,struct image_info *i) {(void)p;i->base=image_base();return 1;}
static void *resolve(void *lib,const char *name) {
    (void)lib;assert(!strcmp(name,"_Z22pageDevelopment_LoadUIv"));
    return missing?0:(unsigned char *)image_base()+(modern?0xf9f44:0xed990)+wrong_symbol;
}
static void log_line(const struct dashborne_host *h,const char *s) {(void)h;(void)s;}
int main(void) {
    struct dashborne_host h={.abi_version=1,.struct_size=sizeof(h),.resolve=resolve,.log=log_line};
    unsigned int *count=(unsigned int *)(memory+0x359034);
    *count=5;assert(activate(0)==-1);assert(!category_count);
    unsupported=1;assert(activate(&h)==-1);assert(*count==5);unsupported=0;
    read_error=1;assert(activate(&h)==-1);read_error=0;
    missing=1;assert(activate(&h)==-1);missing=0;
    wrong_symbol=4;assert(activate(&h)==-1);wrong_symbol=0;
    readonly=1;assert(activate(&h)==-1);readonly=0;
    *count=7;assert(activate(&h)==-1);assert(*count==7);
    *count=5;assert(activate(&h)==0);assert(*count==6);
    assert(activate(&h)==-1);assert(deactivate()==0);assert(*count==5);
    assert(deactivate()==0);assert(*count==5);
    *count=6;assert(activate(&h)==0);assert(deactivate()==0);assert(*count==6);
    *count=5;assert(activate(&h)==0);*count=4;assert(deactivate()==0);assert(*count==4);
    modern=1;db_stock_cached_version=0;
    count=(unsigned int *)((unsigned char *)image_base()+0x3590f4);
    *count=5;assert(!activate(&h) && *count==6);
    assert(activate(&h)==-1);assert(!deactivate() && *count==5);
    readonly=1;assert(activate(&h)==-1 && *count==5);readonly=0;
    *count=7;assert(activate(&h)==-1 && *count==7);
    puts("development-menu: compatibility checks and reversible activation passed");
}
