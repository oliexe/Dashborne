#include "engine.h"
extern void *dlopen(const char *, int);
extern int write(int, const void *, unsigned int);
static void log_line(const char *s) { unsigned int n=0;while(s[n])++n;(void)write(1,s,n); }
int main(int argc, char **argv) {
    if (!extensions_recover(log_line)) return 10;
    if (argc>1 && argv[1][0]=='r') {log_line("PASS: journal recovery\n");return 0;}
    void *core=dlopen("/mnt/extsd/Dashborne/engine-smoke/fake-core.so",2);
    if (!core) return 11;
    const char *ids[]={"lifecycle-example","overlay-probe"};
    if (extensions_start(ids,2,core,core,(const char *)0,log_line)!=2) return 12;
    if (argc>1 && argv[1][0]=='l') {log_line("PASS: leaving journaled mount for next-process recovery\n");return 0;}
    if (!extensions_stop()) return 13;
    log_line("PASS: native ABI, read-only overlay and reverse cleanup\n");return 0;
}
