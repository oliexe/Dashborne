#include "state.h"
#include "config.h"
#include "../extensions/engine.h"
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
static int valid_id(const char *s) {
    unsigned int n = 0;
    for (; *s; ++s, ++n)
        if (!((*s >= 'a' && *s <= 'z') || (*s >= '0' && *s <= '9') || *s == '-' || *s == '_')) return 0;
    return n > 0 && n < 32;
}
static unsigned char *load_limit(const char *path, unsigned int *size, unsigned int limit) {
    int fd = open(path, 0);
    if (fd < 0) return (void *)0;
    unsigned char *data = malloc(limit + 1);
    if (!data) { close(fd); return (void *)0; }
    unsigned int used = 0; int n;
    while ((n = read(fd, data + used, limit + 1 - used)) > 0) {
        used += (unsigned int)n;
        if (used > limit) break;
    }
    close(fd);
    if (n < 0 || used > limit) { free(data); return (void *)0; }
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
    unsigned char *patch = load_limit(path, &size, 16412);
    int ok = base && patch && patch_layout(base, base_size, patch, size, &out, &out_size);
    free(out); free(patch); return ok;
}
void state_restore(void) { (void)umount2(DASHBORNE_TARGET, 2); }
void state_load(struct settings *state) {
    *state = (struct settings){0};
    /* Clear a previous Dashborne file override when reloading the SD app. */
    (void)umount2(DASHBORNE_TARGET, 2);
    unsigned int n = 0, base_size = 0;
    unsigned char *base = load_limit(DASHBORNE_TARGET, &base_size, LIMIT);
    unsigned char *catalog = load_limit(DASHBORNE_ROOT "/patches/catalog.tsv", &n, 16384);
    if (catalog && n < 16384) {
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
            char *kind = description;
            while (*kind && *kind != '\t') ++kind;
            int native = 0;
            if (*kind) {
                *kind++ = 0;
                if (!equal(kind, "native")) continue;
                native = 1;
            }
            if (!valid_id(entry) || !*title) continue;
            unsigned int i;
            for (i = 0; i < state->count; ++i) if (equal(state->patches[i].id, entry)) break;
            if (i != state->count) continue;
            struct patch_entry *p = &state->patches[state->count++];
            copy(p->id, entry, sizeof(p->id)); copy(p->title, title, sizeof(p->title));
            copy(p->description, description, sizeof(p->description));
            p->native = native;
            p->compatible = native ? extension_probe(p->id) : compatible(base, base_size, p->id);
        }
    }
    free(catalog); free(base);
    state->config_ok=config_load();
    if(state->config_ok) {
        char value[1024];
        if(config_get("adb.enabled",value,sizeof(value)))state->wifi_adb=equal(value,"1");
        if(config_get("loader.autolaunch",value,sizeof(value)))state->loader_autolaunch=equal(value,"1");
        config_get("loader.app",state->loader_app,sizeof(state->loader_app));
        for(unsigned int i=0;i<state->count;++i) {
            char key[64]="tweak.";unsigned int at=copy(key+6,state->patches[i].id,sizeof(key)-6)+6;
            copy(key+at,".enabled",sizeof(key)-at);
            if(config_get(key,value,sizeof(value)))state->patches[i].enabled=state->patches[i].compatible && equal(value,"1");
        }
    }
    config_clear();
}
int state_save(const struct settings *state) {
    if(state->count>MAX_PATCHES || !config_load())return 0;
    int ok=config_set("adb.enabled",state->wifi_adb ? "1" : "0") &&
        config_set("loader.autolaunch",state->loader_autolaunch ? "1" : "0") && config_set("loader.app",state->loader_app);
    for(unsigned int i=0;ok && i<state->count;++i) {
        if(!valid_id(state->patches[i].id)){ok=0;break;}
        /* Preserve choices for temporarily unavailable/incompatible tweaks. */
        if(!state->patches[i].compatible)continue;
        char key[64]="tweak.";unsigned int at=copy(key+6,state->patches[i].id,sizeof(key)-6)+6;
        copy(key+at,".enabled",sizeof(key)-at);
        ok=config_set(key,state->patches[i].enabled ? "1" : "0");
    }
    ok=ok && config_commit();config_clear();return ok;
}
int state_apply(const struct settings *state, void (*log)(const char *)) {
    if (state->count > MAX_PATCHES) return -1;
    int core=extension_probe("extra-menu");
    unsigned int chosen = core?1:0;
    for (unsigned int i = 0; i < state->count; ++i) chosen += state->patches[i].enabled && !state->patches[i].native;
    /* A failed extension activation may return to configuration after a layout
       override was mounted. Always compose from the installed file again. */
    (void)umount2(DASHBORNE_TARGET, 2);
    if (!chosen) { log("patches: none selected\n"); return 0; }
    unsigned int base_size = 0, loaded = 0;
    unsigned char *base = load_limit(DASHBORNE_TARGET, &base_size, LIMIT);
    struct layout_patch patches[MAX_PATCHES+1] = {{0}};
    int success = base != (void *)0;
    if(core) {
        patches[loaded].data=load_limit(DASHBORNE_ROOT "/extensions/extra-menu/assets/dashboard.dbp",
                                       &patches[loaded].size,16412);
        if(!patches[loaded].data)success=0;
        ++loaded;
    }
    for (unsigned int i = 0; success && i < state->count; ++i) {
        const struct patch_entry *entry = &state->patches[i];
        if (!entry->enabled || entry->native) continue;
        if (!entry->compatible || !valid_id(entry->id)) { success = 0; break; }
        char path[256]; patch_path(path, entry->id);
        patches[loaded].data = load_limit(path, &patches[loaded].size, 16412);
        if (!patches[loaded].data) success = 0;
        ++loaded;
    }
    unsigned char *result = (void *)0; unsigned int result_size = 0;
    if (success) success = patch_layout_many(base, base_size, patches, loaded, &result, &result_size);
    for (unsigned int i = 0; i < loaded; ++i) free((void *)patches[i].data);
    free(base);
    if (success) success = save(DASHBORNE_ROOT "/generated.ftu", DASHBORNE_ROOT "/generated.ftu.next", result, result_size);
    free(result);
    if (success) success = mount(DASHBORNE_ROOT "/generated.ftu", DASHBORNE_TARGET, (const char *)0, 4096, (const void *)0) == 0;
    log(success ? "patches: selected patches applied\n" : "patches: apply failed\n");
    return success ? (int)chosen : -1;
}
