#include <assert.h>
#include "../receiver.h"
static unsigned int mask,gate;
static int gate_fail,mask_fail,flush_fail;
static int protect_mask(int write) {return write && mask_fail?-1:0;}
static int protect_gate(int write) {return write && gate_fail?-1:0;}
static int flush(void *a,void *b) {(void)a;(void)b;return flush_fail?-1:0;}
static struct receiver_patches fresh(void) {
    mask=0xe3a01012;gate=0x13530001;gate_fail=mask_fail=flush_fail=0;
    return (struct receiver_patches){
        {&mask,mask,0,0,protect_mask,flush},
        {&gate,gate,0x13530000,0,protect_gate,flush}};
}
int main(void) {
    struct receiver_patches p=fresh();
    assert(!receiver_mode(&p,MIRROR_AIRPLAY) && mask==0xe3001400 && gate==0x13530001);
    assert(receiver_mode(&p,MIRROR_MIRACAST)); /* No stacking active modes. */
    assert(!receiver_mode(&p,MIRROR_STOCK));
    assert(!receiver_mode(&p,MIRROR_MIRACAST) && mask==0xe3001800 && gate==0x13530000);
    mask_fail=1;assert(receiver_mode(&p,MIRROR_STOCK) && p.mask.touched && !p.network_gate.touched);
    mask_fail=0;assert(!receiver_mode(&p,MIRROR_STOCK));
    p=fresh();mask_fail=1;assert(receiver_mode(&p,MIRROR_MIRACAST) && p.network_gate.touched && !p.mask.touched);
    assert(!receiver_mode(&p,MIRROR_STOCK) && gate==0x13530001);
    p=fresh();gate_fail=1;assert(receiver_mode(&p,MIRROR_MIRACAST) && mask==p.mask.original);
    assert(!receiver_mode(&p,MIRROR_AIRPLAY)); /* Optional P2P guard does not affect AirPlay. */
    assert(!receiver_mode(&p,MIRROR_STOCK));
    p=fresh();flush_fail=1;assert(receiver_mode(&p,MIRROR_MIRACAST) && p.network_gate.touched);
    assert(receiver_mode(&p,MIRROR_STOCK));flush_fail=0;assert(!receiver_mode(&p,MIRROR_STOCK));
    assert(receiver_mode(&p,99) && mask==p.mask.original && gate==p.network_gate.original);
}
