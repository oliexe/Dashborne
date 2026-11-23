/* DBP1: an original additive patch applied to the device's own FTU layout. */
#include "patch.h"
typedef __SIZE_TYPE__ usize;
extern void *malloc(usize);
extern void free(void *);
extern unsigned long crc32(unsigned long, const unsigned char *, unsigned int);
extern int uncompress(unsigned char *, unsigned long *, const unsigned char *, unsigned long);
extern int compress2(unsigned char *, unsigned long *, const unsigned char *, unsigned long, int);
extern unsigned long compressBound(unsigned long);
#define LIMIT (2u * 1024u * 1024u)
static unsigned int get32(const unsigned char *p) {
    return (unsigned int)p[0] | (unsigned int)p[1] << 8 |
           (unsigned int)p[2] << 16 | (unsigned int)p[3] << 24;
}
static void put32(unsigned char *p, unsigned int n) {
    for (int i = 0; i < 4; ++i) { p[i] = (unsigned char)n; n >>= 8; }
}
static void copy(unsigned char *to, const unsigned char *from, unsigned int n) {
    for (unsigned int i = 0; i < n; ++i) to[i] = from[i];
}
static int equal(const unsigned char *a, const char *b, unsigned int n) {
    for (unsigned int i = 0; i < n; ++i) if (a[i] != (unsigned char)b[i]) return 0;
    return 1;
}
int patch_layout(const unsigned char *base, unsigned int size,
                 const unsigned char *patch, unsigned int length,
                 unsigned char **output, unsigned int *output_size) {
    *output = (void *)0; *output_size = 0;
    if (size < 32 || size > LIMIT || length < 29 || length > 16412 ||
        !equal(patch, "DBP1", 4) || !equal(base, "ZKSW\xaa\x55", 6) ||
        !equal(base + size - 6, "\x55\xaaZKSW", 6)) return 0;
    unsigned int plain_size = get32(patch + 12), added = get32(patch + 20);
    if (size != get32(patch + 4) || crc32(0, base, size) != get32(patch + 8) ||
        plain_size < 2 || plain_size > LIMIT || added != length - 28 ||
        crc32(0, patch + 28, added) != get32(patch + 24) || patch[28] != ',') return 0;
    unsigned int offset = get32(base + 6), packed = get32(base + 14);
    if (offset < 18 || offset > 256 || offset + 8 > size - 6 || packed < 16 ||
        packed - 16 != size - 6 - (offset + 8) || get32(base + 10) != plain_size + 8) return 0;
    unsigned char *plain = malloc(plain_size + added);
    if (!plain) return 0;
    unsigned long unpacked = plain_size;
    if (uncompress(plain, &unpacked, base + offset + 8, packed - 16) ||
        unpacked != plain_size || crc32(0, plain, plain_size) != get32(patch + 16) ||
        plain[0] != '{' || plain[plain_size - 1] != '}') { free(plain); return 0; }
    copy(plain + plain_size - 1, patch + 28, added);
    plain[plain_size + added - 1] = '}';
    unsigned long capacity = compressBound(plain_size + added);
    unsigned char *result = malloc(offset + 8 + capacity + 6);
    if (!result) { free(plain); return 0; }
    copy(result, base, offset + 8);
    if (compress2(result + offset + 8, &capacity, plain, plain_size + added, 9)) {
        free(plain); free(result); return 0;
    }
    free(plain);
    put32(result + 10, plain_size + added + 8);
    put32(result + 14, (unsigned int)capacity + 16);
    copy(result + offset + 8 + capacity, base + size - 6, 6);
    *output = result; *output_size = offset + 8 + (unsigned int)capacity + 6;
    return 1;
}
