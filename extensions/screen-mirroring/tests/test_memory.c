#include <assert.h>
#include <stdio.h>
#include "../memory.h"
static int writable, fail_open, fail_close, fail_flush, flushes;
static int protect(int value) {
    if((value && fail_open) || (!value && fail_close))return -1;
    writable=value;return 0;
}
static int flush(void *a,void *b) {assert(writable && (char *)b-(char *)a==4);++flushes;return fail_flush?-1:0;}
static unsigned int word;
static struct instruction_patch setup(void) {
    writable=fail_open=fail_close=fail_flush=flushes=0;word=0xe3a01012;
    return (struct instruction_patch){&word,0xe3a01012,0xe3001400,0,protect,flush};
}
int main(void) {
    struct instruction_patch p=setup();
    assert(!instruction_apply(&p) && word==p.replacement && !writable && flushes==1);
    assert(instruction_apply(&p));
    assert(!instruction_restore(&p) && word==p.original && !writable && flushes==2);
    assert(!instruction_restore(&p) && flushes==2);
    p=setup();word=0;assert(instruction_apply(&p) && !p.touched && !writable);
    p=setup();fail_open=1;assert(instruction_apply(&p) && word==p.original && !p.touched);
    p=setup();fail_close=1;assert(instruction_apply(&p) && p.touched && writable);
    fail_close=0;assert(!instruction_restore(&p) && word==p.original && !writable);
    p=setup();fail_flush=1;assert(instruction_apply(&p) && p.touched && !writable);
    assert(instruction_restore(&p) && p.touched);fail_flush=0;
    assert(!instruction_restore(&p) && !p.touched && word==p.original);
    p=setup();assert(!instruction_apply(&p));fail_open=1;
    assert(instruction_restore(&p) && p.touched && word==p.replacement);
    fail_open=0;assert(!instruction_restore(&p));
    p=setup();assert(!instruction_apply(&p));word=0;
    assert(instruction_restore(&p) && word==0 && p.touched);
    word=p.replacement;assert(!instruction_restore(&p));
    puts("memory patch: guarded apply, rollback, retry, cache/protection failures passed");
}
