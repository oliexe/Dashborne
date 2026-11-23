#ifndef SCREEN_MIRRORING_MEMORY_H
#define SCREEN_MIRRORING_MEMORY_H
struct instruction_patch {
    unsigned int *word;
    unsigned int original, replacement;
    int touched;
    int (*protect)(int writable);
    int (*flush)(void *begin, void *end);
};
int instruction_apply(struct instruction_patch *p);
int instruction_restore(struct instruction_patch *p);
#endif
