#ifndef VEHICLE_MEMORY_H
#define VEHICLE_MEMORY_H
#include "dashborne_extension.h"
typedef __UINTPTR_TYPE__ mem_address;
struct memory_edit {void *at;unsigned char before[32],after[32];unsigned int size;int applied,protection;};
int memory_prepare(struct memory_edit *,void *,const void *,const void *,unsigned int);
int memory_apply(struct memory_edit *);
int memory_restore(struct memory_edit *);
#endif
