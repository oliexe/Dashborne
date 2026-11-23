/* Versioned, guarded edits to the device's own FTU layout. */
#include "patch.h"
typedef __SIZE_TYPE__ usize;
extern void *malloc(usize);
extern void free(void *);
extern unsigned long crc32(unsigned long, const unsigned char *, unsigned int);
extern int uncompress(unsigned char *, unsigned long *, const unsigned char *, unsigned long);
extern int compress2(unsigned char *, unsigned long *, const unsigned char *, unsigned long, int);
extern unsigned long compressBound(unsigned long);
#define LIMIT (2u * 1024u * 1024u)
#define PAYLOAD_LIMIT 16384u
#define EDIT_LIMIT 1024u
struct edit {
    unsigned int offset, size;
    const unsigned char *replacement;
};
static unsigned int get32(const unsigned char *p) {
    return (unsigned int)p[0] | (unsigned int)p[1] << 8 | (unsigned int)p[2] << 16 |
           (unsigned int)p[3] << 24;
}
static void put32(unsigned char *p, unsigned int n) {
    for (int i = 0; i < 4; ++i) {
        p[i] = (unsigned char)n;
        n >>= 8;
    }
}
static void copy(unsigned char *to, const unsigned char *from, unsigned int n) {
    for (unsigned int i = 0; i < n; ++i)
        to[i] = from[i];
}
static int equal(const unsigned char *a, const unsigned char *b, unsigned int n) {
    for (unsigned int i = 0; i < n; ++i)
        if (a[i] != b[i])
            return 0;
    return 1;
}
int patch_layout_many(const unsigned char *base, unsigned int size,
                      const struct layout_patch *patches, unsigned int count,
                      unsigned char **output, unsigned int *output_size) {
    *output = (void *)0;
    *output_size = 0;
    if (!base || !patches || !count || count > 64 || size < 32 || size > LIMIT ||
        !equal(base, (const unsigned char *)"ZKSW\xaa\x55", 6) ||
        !equal(base + size - 6, (const unsigned char *)"\x55\xaaZKSW", 6))
        return 0;
    unsigned int offset = get32(base + 6), packed = get32(base + 14);
    unsigned int declared = get32(base + 10);
    if (declared < 10 || declared > LIMIT + 8 || offset < 18 || offset > 256 ||
        offset + 8 > size - 6 || packed < 16 || packed - 16 != size - 6 - (offset + 8))
        return 0;
    unsigned int plain_size = declared - 8;
    unsigned char *plain = malloc(plain_size + PAYLOAD_LIMIT), *result = (void *)0;
    struct edit *edits = malloc(EDIT_LIMIT * sizeof(*edits));
    int success = 0;
    if (!plain || !edits)
        goto done;
    unsigned long unpacked = plain_size;
    if (uncompress(plain, &unpacked, base + offset + 8, packed - 16) || unpacked != plain_size ||
        plain[0] != '{' || plain[plain_size - 1] != '}')
        goto done;
    unsigned int base_crc = (unsigned int)crc32(0, base, size);
    unsigned int plain_crc = (unsigned int)crc32(0, plain, plain_size);
    unsigned int added = 0, edit_count = 0;
    /* Every guard is checked against the original, before any edit is applied. */
    for (unsigned int i = 0; i < count; ++i) {
        const unsigned char *p = patches[i].data;
        unsigned int length = patches[i].size;
        if (!p || length < 29 || length > PAYLOAD_LIMIT + 28 ||
            !equal(p, (const unsigned char *)"DBP", 3) || (p[3] != '1' && p[3] != '2') ||
            get32(p + 4) != size || get32(p + 8) != base_crc || get32(p + 12) != plain_size ||
            get32(p + 16) != plain_crc || get32(p + 20) != length - 28 ||
            crc32(0, p + 28, length - 28) != get32(p + 24))
            goto done;
        if (p[3] == '1') {
            if (p[28] != ',' || length - 28 > PAYLOAD_LIMIT - added)
                goto done;
            added += length - 28;
            continue;
        }
        unsigned int at = 28;
        while (at < length) {
            if (length - at < 8 || edit_count == EDIT_LIMIT)
                goto done;
            unsigned int start = get32(p + at), n = get32(p + at + 4);
            at += 8;
            if (!n || n > 256 || n > (length - at) / 2 || start >= plain_size ||
                n > plain_size - start || !equal(plain + start, p + at, n))
                goto done;
            for (unsigned int j = 0; j < edit_count; ++j)
                if (start < edits[j].offset + edits[j].size && edits[j].offset < start + n)
                    goto done;
            edits[edit_count++] = (struct edit){start, n, p + at + n};
            at += 2 * n;
        }
    }
    for (unsigned int i = 0; i < edit_count; ++i)
        copy(plain + edits[i].offset, edits[i].replacement, edits[i].size);
    unsigned int at = plain_size - 1;
    for (unsigned int i = 0; i < count; ++i)
        if (patches[i].data[3] == '1') {
            copy(plain + at, patches[i].data + 28, patches[i].size - 28);
            at += patches[i].size - 28;
        }
    plain[plain_size + added - 1] = '}';
    unsigned long capacity = compressBound(plain_size + added);
    result = malloc(offset + 8 + capacity + 6);
    if (!result)
        goto done;
    copy(result, base, offset + 8);
    if (compress2(result + offset + 8, &capacity, plain, plain_size + added, 9))
        goto done;
    put32(result + 10, plain_size + added + 8);
    put32(result + 14, (unsigned int)capacity + 16);
    copy(result + offset + 8 + capacity, base + size - 6, 6);
    *output = result;
    *output_size = offset + 8 + (unsigned int)capacity + 6;
    result = (void *)0;
    success = 1;
done:
    free(result);
    free(edits);
    free(plain);
    return success;
}
int patch_layout(const unsigned char *base, unsigned int size, const unsigned char *patch,
                 unsigned int length, unsigned char **output, unsigned int *output_size) {
    const struct layout_patch one = {patch, length};
    return patch_layout_many(base, size, &one, 1, output, output_size);
}
