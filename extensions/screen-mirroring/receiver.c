#include "receiver.h"
int receiver_mode(struct receiver_patches *p,int protocol) {
    if(protocol==MIRROR_STOCK) {
        /* Attempt both restorations even if one fails; retain touched state. */
        int mask=instruction_restore(&p->mask);
        int gate=instruction_restore(&p->network_gate);
        return mask || gate?-1:0;
    }
    if(!protocol_valid(protocol) || p->mask.touched || p->network_gate.touched)return -1;
    if(protocol==MIRROR_MIRACAST && instruction_apply(&p->network_gate))return -1;
    p->mask.replacement=protocol_instruction(protocol);
    return instruction_apply(&p->mask);
}
