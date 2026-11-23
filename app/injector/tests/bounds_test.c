/* Exercise both binary patch formats with ASan/UBSan and short buffers. */
#include "../patch.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
static void put(unsigned char *p, unsigned int n) {
    for (int i = 0; i < 4; ++i) {
        p[i] = (unsigned char)n;
        n >>= 8;
    }
}
static void apply(const unsigned char *b, unsigned int bn, const unsigned char *p, unsigned int pn,
                  int expected) {
    unsigned char *out = NULL;
    unsigned int n = 0;
    int ok = patch_layout(b, bn, p, pn, &out, &n);
    if (expected >= 0)
        assert(ok == expected);
    if (!ok)
        assert(!out && !n);
    free(out);
}
int main(void) {
    const unsigned char plain[] = "{\"id\":0,\"fontSize\":28}";
    unsigned int plain_size = sizeof(plain) - 1;
    unsigned char base[256] = {0}, patch[64] = {0};
    unsigned long compressed = 200;
    memcpy(base, "ZKSW\xaa\x55", 6);
    put(base + 6, 18);
    put(base + 10, plain_size + 8);
    assert(!compress2(base + 26, &compressed, plain, plain_size, 9));
    put(base + 14, (unsigned int)compressed + 16);
    memcpy(base + 26 + compressed, "\x55\xaaZKSW", 6);
    unsigned int size = 32 + (unsigned int)compressed;
    memcpy(patch, "DBP2", 4);
    put(patch + 4, size);
    put(patch + 8, (unsigned int)crc32(0, base, size));
    put(patch + 12, plain_size);
    put(patch + 16, (unsigned int)crc32(0, plain, plain_size));
    put(patch + 20, 12);
    put(patch + 28, plain_size - 3);
    put(patch + 32, 2);
    memcpy(patch + 36, "2832", 4);
    put(patch + 24, (unsigned int)crc32(0, patch + 28, 12));
    apply(base, size, patch, 40, 1);
    for (unsigned int n = 0; n < size; ++n)
        apply(base, n, patch, 40, 0);
    for (unsigned int n = 0; n < 40; ++n)
        apply(base, size, patch, n, 0);
    unsigned int seed = 7;
    for (int i = 0; i < 10000; ++i) {
        unsigned char changed[40];
        memcpy(changed, patch, 40);
        seed = seed * 1664525u + 1013904223u;
        unsigned int at = seed % 40;
        changed[at] ^= (unsigned char)(1u << ((seed >> 12) % 8));
        if (at >= 28 && (i & 1))
            put(changed + 24, (unsigned int)crc32(0, changed + 28, 12));
        apply(base, size, changed, 40, -1);
    }
    return 0;
}
