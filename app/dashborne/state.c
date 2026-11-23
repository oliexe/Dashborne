#include "state.h"
#include "../injector/patch.h"
typedef __SIZE_TYPE__ usize;
extern void *malloc(usize);
extern void free(void *);
extern int open(const char *, int, ...);
extern int read(int, void *, unsigned int);
extern int write(int, const void *, unsigned int);
extern int close(int);
extern int fsync(int);
extern int rename(const char *, const char *);
extern int unlink(const char *);
extern int mount(const char *, const char *, const char *, unsigned long, const void *);
extern int umount2(const char *, int);
extern unsigned long crc32(unsigned long, const unsigned char *, unsigned int);
#ifndef DASHBORNE_ROOT
#define DASHBORNE_ROOT "/mnt/extsd/Dashborne"
#endif
#ifndef DASHBORNE_TARGET
#define DASHBORNE_TARGET "/res/ui/dash1act.ftu"
#endif
#define LIMIT (2u * 1024u * 1024u)
static unsigned int copy(char *to, const char *from, unsigned int cap) {
    unsigned int n = 0;
    while (from[n] && n + 1 < cap) { to[n] = from[n]; ++n; }
    to[n] = 0; return n;
}
static int equal(const char *a, const char *b) {
    while (*a && *a == *b) { ++a; ++b; } return *a == *b;
}
static int starts(const char *a, const char *b) {
    while (*b) if (*a++ != *b++) return 0;
    return 1;
}
static int valid_id(const char *s) {
    unsigned int n = 0;
    for (; *s; ++s, ++n)
        if (!((*s >= 'a' && *s <= 'z') || (*s >= '0' && *s <= '9') || *s == '-' || *s == '_')) return 0;
    return n > 0 && n < 32;
}
static unsigned char *load(const char *path, unsigned int *size) {
    int fd = open(path, 0);
    if (fd < 0) return (void *)0;
    unsigned char *data = malloc(LIMIT + 1);
    if (!data) { close(fd); return (void *)0; }
    unsigned int used = 0; int n;
    while ((n = read(fd, data + used, LIMIT + 1 - used)) > 0) {
        used += (unsigned int)n;
        if (used > LIMIT) break;
    }
    close(fd);
    if (n < 0 || used > LIMIT) { free(data); return (void *)0; }
    data[used] = 0; *size = used; return data;
}
static int save(const char *path, const char *pending, const unsigned char *data, unsigned int size) {
    int fd = open(pending, 1 | 0100 | 01000, 0644);
    if (fd < 0) return 0;
    unsigned int at = 0;
    while (at < size) {
        int n = write(fd, data + at, size - at);
        if (n <= 0) break;
        at += (unsigned int)n;
    }
    int ok = at == size && fsync(fd) == 0;
    if (close(fd)) ok = 0;
    if (ok && rename(pending, path) == 0) return 1;
    unlink(pending); return 0;
}
static char *line(char **cursor) {
    if (!**cursor) return (void *)0;
    char *start = *cursor;
    while (**cursor && **cursor != '\n') ++*cursor;
    if (**cursor) *(*cursor)++ = 0;
    return start;
}
static void patch_path(char *path, const char *id) {
    unsigned int n = copy(path, DASHBORNE_ROOT "/patches/", 256);
    n += copy(path + n, id, 256 - n);
    copy(path + n, ".dbp", 256 - n);
}
static int compatible(const unsigned char *base, unsigned int base_size, const char *id) {
    char path[256]; patch_path(path, id);
    unsigned int size = 0, out_size = 0; unsigned char *out = (void *)0;
    unsigned char *patch = load(path, &size);
    int ok = base && patch && patch_layout(base, base_size, patch, size, &out, &out_size);
    free(out); free(patch); return ok;
}
void state_load(struct settings *state) {
    *state = (struct settings){0};
    /* Clear a previous Dashborne file override when reloading the SD app. */
    (void)umount2(DASHBORNE_TARGET, 2);
    unsigned int n = 0, base_size = 0;
    unsigned char *base = load(DASHBORNE_TARGET, &base_size);
    unsigned char *catalog = load(DASHBORNE_ROOT "/patches/catalog.tsv", &n);
    if (catalog && n < 8192) {
        char *cursor = (char *)catalog, *entry;
        while ((entry = line(&cursor)) && state->count < MAX_PATCHES) {
            char *title = entry;
            while (*title && *title != '\t') ++title;
            if (!*title) continue;
            *title++ = 0;
            char *description = title;
            while (*description && *description != '\t') ++description;
            if (!*description) continue;
            *description++ = 0;
            if (!valid_id(entry) || !*title) continue;
            unsigned int i;
            for (i = 0; i < state->count; ++i) if (equal(state->patches[i].id, entry)) break;
            if (i != state->count) continue;
            struct patch_entry *p = &state->patches[state->count++];
            copy(p->id, entry, sizeof(p->id)); copy(p->title, title, sizeof(p->title));
            copy(p->description, description, sizeof(p->description));
            p->compatible = compatible(base, base_size, p->id);
        }
    }
    free(catalog); free(base);
    unsigned char *config = load(DASHBORNE_ROOT "/settings.conf", &n);
    if (config && n < 8192) {
        char *cursor = (char *)config, *entry;
        while ((entry = line(&cursor))) {
            if (equal(entry, "wifi_adb=1")) state->wifi_adb = 1;
            if (equal(entry, "wifi_adb=0")) state->wifi_adb = 0;
            if (!starts(entry, "patch.")) continue;
            char *value = entry + 6;
            while (*value && *value != '=') ++value;
            if (!*value) continue;
            *value++ = 0;
            for (unsigned int i = 0; i < state->count; ++i)
                if (equal(entry + 6, state->patches[i].id))
                    state->patches[i].enabled = state->patches[i].compatible && equal(value, "1");
        }
    }
    free(config);
}
int state_save(const struct settings *state) {
    if (state->count > MAX_PATCHES) return 0;
    char data[1024];
    unsigned int n = copy(data, state->wifi_adb ? "wifi_adb=1\n" : "wifi_adb=0\n", sizeof(data));
    for (unsigned int i = 0; i < state->count; ++i) {
        if (!valid_id(state->patches[i].id)) return 0;
        n += copy(data + n, "patch.", sizeof(data) - n);
        n += copy(data + n, state->patches[i].id, sizeof(data) - n);
        n += copy(data + n, state->patches[i].enabled && state->patches[i].compatible ? "=1\n" : "=0\n", sizeof(data) - n);
    }
    return save(DASHBORNE_ROOT "/settings.conf", DASHBORNE_ROOT "/settings.conf.next", (unsigned char *)data, n);
}
static void put32(unsigned char *p, unsigned int n) {
    for (int i = 0; i < 4; ++i) { p[i] = (unsigned char)n; n >>= 8; }
}
int state_apply(const struct settings *state, void (*log)(const char *)) {
    if (state->count > MAX_PATCHES) return -1;
    unsigned int chosen = 0;
    for (unsigned int i = 0; i < state->count; ++i) chosen += !!state->patches[i].enabled;
    if (!chosen) { log("patches: none selected\n"); return 0; }
    unsigned int base_size = 0, added = 0;
    unsigned char *base = load(DASHBORNE_TARGET, &base_size), *combined = malloc(16412);
    int success = base && combined;
    for (unsigned int i = 0; success && i < state->count; ++i) {
        const struct patch_entry *entry = &state->patches[i];
        if (!entry->enabled) continue;
        if (!entry->compatible || !valid_id(entry->id)) { success = 0; break; }
        char path[256]; patch_path(path, entry->id);
        unsigned int size = 0, test_size = 0; unsigned char *test = (void *)0;
        unsigned char *p = load(path, &size);
        success = p && patch_layout(base, base_size, p, size, &test, &test_size);
        free(test);
        if (success && size - 28 <= 16384 - added) {
            for (unsigned int j = 0; j < 20; ++j) combined[j] = p[j];
            for (unsigned int j = 28; j < size; ++j) combined[28 + added + j - 28] = p[j];
            added += size - 28;
        } else success = 0;
        free(p);
    }
    unsigned char *result = (void *)0; unsigned int result_size = 0;
    if (success) {
        put32(combined + 20, added);
        put32(combined + 24, (unsigned int)crc32(0, combined + 28, added));
        success = patch_layout(base, base_size, combined, added + 28, &result, &result_size);
    }
    free(base); free(combined);
    if (success) success = save(DASHBORNE_ROOT "/generated.ftu", DASHBORNE_ROOT "/generated.ftu.next", result, result_size);
    free(result);
    if (success) success = mount(DASHBORNE_ROOT "/generated.ftu", DASHBORNE_TARGET, (const char *)0, 4096, (const void *)0) == 0;
    log(success ? "patches: selected patches applied\n" : "patches: apply failed\n");
    return success ? (int)chosen : -1;
}
