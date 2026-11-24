#ifndef MIRROR_PROTOCOL_H
#define MIRROR_PROTOCOL_H
enum { MIRROR_STOCK, MIRROR_AIRPLAY, MIRROR_MIRACAST };
#define MIRROR_SUPPORT(p) (1u << (p))
static inline int protocol_valid(int p) {return p==MIRROR_AIRPLAY || p==MIRROR_MIRACAST;}
static inline int protocol_link_type(int p) {return p==MIRROR_MIRACAST?11:10;}
static inline unsigned int protocol_instruction(int p) {
    /* ARM MOVW r1: exclusive receiver masks, AirPlay 0x400 / Miracast 0x800. */
    return p==MIRROR_MIRACAST?0xe3001800u:0xe3001400u;
}
#endif
