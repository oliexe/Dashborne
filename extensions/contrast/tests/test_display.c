#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "../display.h"
static int contrast=50,luminance=50,fail_op,fail_count,opens,closes,writes;
int fake_open(const char *path,int flags,...) { assert(!strcmp(path,"/dev/disp")); assert(flags==2); opens++;return 7; }
int fake_close(int fd) {assert(fd==7);closes++;return 0;}
int fake_ioctl(int fd,unsigned long op,...) {
    assert(fd==7);va_list ap;va_start(ap,op);unsigned long *args=va_arg(ap,unsigned long *);va_end(ap);
    assert(args[0]==0 && args[2]==0 && args[3]==0);
    if((int)op==fail_op && fail_count) {--fail_count;return -1;}
    switch(op) {
    case 0x193:return contrast;case 0x191:return luminance;
    case 0x192:contrast=(int)args[1];writes++;return 0;
    case 0x190:luminance=(int)args[1];writes++;return 0;
    default:assert(0);return -1;
    }
}
#define open fake_open
#define close fake_close
#define ioctl fake_ioctl
#include "../display.c"
int main(void) {
    struct display_state s={0};
    assert(!display_restore(&s) && opens==0);
    assert(display_apply(&s,50,50)==-1 && opens==0);
    assert(!display_capture(&s) && !writes && !s.enabled);
    assert(!display_apply(&s,58,46) && contrast==58 && luminance==46 && s.enabled);
    assert(display_apply(&s,101,46)==-1 && contrast==58);
    assert(!display_restore(&s) && contrast==50 && luminance==50 && !s.enabled);
    fail_op=0x190;fail_count=1;
    assert(display_apply(&s,60,40)==-1 && contrast==50 && luminance==50 && !s.touched);
    fail_count=2;
    assert(display_apply(&s,60,40)==-2 && s.touched);
    assert(!display_restore(&s) && !s.touched);
    fail_op=0x191;fail_count=1;memset(&s,0,sizeof(s));
    assert(display_capture(&s)==-1 && !s.captured);
    assert(opens==closes);
    puts("display: capture, limits, readback, partial-write rollback and cleanup retry passed");
}
