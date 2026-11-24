#ifndef MIRROR_RECEIVER_H
#define MIRROR_RECEIVER_H
#include "memory.h"
#include "protocol.h"
struct receiver_patches {struct instruction_patch mask, network_gate;};
int receiver_mode(struct receiver_patches *,int protocol);
#endif
