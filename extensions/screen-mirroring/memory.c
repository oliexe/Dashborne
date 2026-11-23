#include "memory.h"
/* A single aligned ARM instruction; no branch into plugin code and no callbacks
   that could outlive unloading. The caller verifies the full image and mapping. */
int instruction_apply(struct instruction_patch *p) {
    if (!p || !p->word || ((__UINTPTR_TYPE__)p->word & 3) || p->touched ||
        !p->protect || !p->flush || *p->word != p->original) return -1;
    if (p->protect(1)) return -1;
    __atomic_store_n(p->word, p->replacement, __ATOMIC_SEQ_CST);
    p->touched = 1;
    int result = p->flush(p->word, p->word + 1);
    if (p->protect(0)) result = -1;
    return result;
}
int instruction_restore(struct instruction_patch *p) {
    if (!p || !p->touched) return 0;
    /* Do not overwrite another extension's change. Keep cleanup pending. */
    if (*p->word != p->replacement && *p->word != p->original) return -1;
    if (p->protect(1)) return -1;
    __atomic_store_n(p->word, p->original, __ATOMIC_SEQ_CST);
    int result = p->flush(p->word, p->word + 1);
    if (p->protect(0)) result = -1;
    if (!result) p->touched = 0;
    return result;
}
