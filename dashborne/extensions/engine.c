#include "engine.h"
#include "dashborne_extension.h"
#include "../core/services.h"
typedef __SIZE_TYPE__ usize;
extern int open(const char *, int, ...), read(int, void *, unsigned int);
extern int write(int, const void *, unsigned int), close(int), fsync(int), unlink(const char *);
extern int rename(const char *, const char *),
    mount(const char *, const char *, const char *, unsigned long, const void *);
extern int umount2(const char *, int);
extern int *__errno_location(void);
extern char *realpath(const char *, char *);
extern void *dlopen(const char *, int), *dlsym(void *, const char *);
extern int dlclose(void *);
extern char *dlerror(void);
struct image_info {const char *name;void *base;const char *symbol;void *address;};
extern int dladdr(const void *,struct image_info *);
#ifndef DBX_ROOT
#define DBX_ROOT "/mnt/extsd/Dashborne"
#endif
#ifndef DBX_RES
#define DBX_RES "/res"
#endif
#define MAX_EXTENSIONS 66
#define MAX_OVERLAYS 128
#define PATH_SIZE 512
#define JOURNAL DBX_ROOT "/extensions.mounts"
struct loaded_extension {
    struct dashborne_host host;
    char directory[PATH_SIZE];
    void *library;
    const struct dashborne_extension *descriptor;
    int entered;
};
static struct loaded_extension loaded[MAX_EXTENSIONS];
static unsigned int loaded_count, overlay_count;
static char targets[MAX_OVERLAYS][PATH_SIZE];
static const char *protected_file;
static void (*log_line)(const char *);
static int activating, poisoned;
static char startup_activity[96];
static const struct dashborne_host *startup_owner;
/* Built-in services share ownership and rollback with plugins, but have no
   manifest, dlopen handle or extension-directory dependency. */
static const struct dashborne_extension core_services[] = {
    {DASHBORNE_EXTENSION_ABI,sizeof(struct dashborne_extension),"widget-runtime",core_widgets_start,core_widgets_stop},
    {DASHBORNE_EXTENSION_ABI,sizeof(struct dashborne_extension),"extra-menu",core_extra_start,core_extra_stop}
};
#define CORE_COUNT (sizeof(core_services)/sizeof(core_services[0]))
static unsigned int length(const char *s) {
    unsigned int n = 0;
    while (s[n])
        ++n;
    return n;
}
static int equal(const char *a, const char *b) {
    while (*a && *a == *b) {
        ++a;
        ++b;
    }
    return *a == *b;
}
static int prefix(const char *a, const char *b) {
    while (*b)
        if (*a++ != *b++)
            return 0;
    return 1;
}
static int copy(char *to, const char *from, unsigned int cap) {
    unsigned int n = length(from);
    if (n >= cap)
        return 0;
    for (unsigned int i = 0; i <= n; ++i)
        to[i] = from[i];
    return 1;
}
static int valid_id(const char *id) {
    if (!id)
        return 0;
    unsigned int n = 0;
    for (; *id; ++id, ++n)
        if (!((*id >= 'a' && *id <= 'z') || (*id >= '0' && *id <= '9') || *id == '-' || *id == '_'))
            return 0;
    return n && n < 32;
}
#include "activities.inc"
#include "extra.inc"
static int clean_path(const char *path) {
    if (!*path || length(path) >= PATH_SIZE)
        return 0;
    const char *start = path;
    for (const char *p = path;; ++p) {
        if (*p == '\n' || *p == '\r' || *p == '\t')
            return 0;
        if (!*p || *p == '/') {
            if ((p - start == 1 && *start == '.') ||
                (p - start == 2 && start[0] == '.' && start[1] == '.'))
                return 0;
            if (!*p)
                break;
            start = p + 1;
        }
    }
    return 1;
}
static void event(const char *s) {
    if (log_line)
        log_line(s);
}
static int regular_readable(const char *path) {
    int fd = open(path, 0);
    if (fd < 0)
        return 0;
    char b;
    int n = read(fd, &b, 1);
    close(fd);
    return n >= 0;
}
static int target_path(const char *path) {
    char resolved[4096];
    return clean_path(path) && prefix(path, DBX_RES "/") && realpath(path, resolved) &&
           equal(path, resolved) && regular_readable(path);
}
static int save_journal(void) {
    int fd = open(JOURNAL ".next", 1 | 0100 | 01000, 0600);
    if (fd < 0)
        return 0;
    int ok = 1;
    for (unsigned int i = 0; ok && i < overlay_count; ++i) {
        unsigned int n = length(targets[i]), at = 0;
        while (at < n) {
            int wrote = write(fd, targets[i] + at, n - at);
            if (wrote <= 0) {
                ok = 0;
                break;
            }
            at += (unsigned int)wrote;
        }
        if (ok && write(fd, "\n", 1) != 1)
            ok = 0;
    }
    if (fsync(fd))
        ok = 0;
    if (close(fd))
        ok = 0;
    if (ok && !rename(JOURNAL ".next", JOURNAL))
        return 1;
    unlink(JOURNAL ".next");
    return 0;
}
static int remove_overlays(void) {
    int ok = 1;
    while (overlay_count) {
        const char *target = targets[overlay_count - 1];
        if (umount2(target, 0) && *__errno_location() != 22 && *__errno_location() != 2) {
            ok = 0;
            break;
        }
        --overlay_count;
    }
    if (!save_journal())
        ok = 0;
    if (!ok) {
        poisoned = 1;
        event("extensions: overlay cleanup failed; reboot required\n");
    }
    return ok;
}
int extensions_recover(void (*logger)(const char *)) {
    log_line = logger;
    if (loaded_count || poisoned)
        return 0;
    int fd = open(JOURNAL, 0);
    if (fd < 0)
        return *__errno_location() == 2;
    overlay_count = 0;
    unsigned int at = 0;
    char c;
    int n, ok = 1;
    while ((n = read(fd, &c, 1)) > 0) {
        if (overlay_count >= MAX_OVERLAYS || at >= PATH_SIZE - 1) {
            ok = 0;
            break;
        }
        if (c == '\n') {
            targets[overlay_count][at] = 0;
            if (!target_path(targets[overlay_count])) {
                ok = 0;
                break;
            }
            ++overlay_count;
            at = 0;
        } else
            targets[overlay_count][at++] = c;
    }
    close(fd);
    if (n < 0 || at)
        ok = 0;
    if (!ok) {
        poisoned = 1;
        event("extensions: invalid recovery journal; reboot required\n");
        return 0;
    }
    return remove_overlays();
}
static void plugin_log(const struct dashborne_host *host, const char *message) {
    (void)host;
    if (message) {
        event("extension: ");
        event(message);
        event("\n");
    }
}
static int overlay_file(const struct dashborne_host *host, const char *source, const char *target) {
    int owner = 0;
    for (unsigned int i = 0; i < loaded_count; ++i)
        if (host == &loaded[i].host)
            owner = 1;
    if (!owner || !activating || poisoned || !source || !target || *source == '/' ||
        !clean_path(source) || overlay_count == MAX_OVERLAYS || !target_path(target) ||
        (protected_file && equal(target, protected_file)))
        return -1;
    for (unsigned int i = 0; i < overlay_count; ++i)
        if (equal(target, targets[i]))
            return -1;
    char path[PATH_SIZE], resolved[4096];
    unsigned int root = length(host->extension_directory), n = length(source);
    if (root + n + 2 > sizeof(path))
        return -1;
    copy(path, host->extension_directory, sizeof(path));
    path[root] = '/';
    copy(path + root + 1, source, sizeof(path) - root - 1);
    if (!realpath(path, resolved) || !prefix(resolved, host->extension_directory) ||
        resolved[root] != '/' || !regular_readable(resolved))
        return -1;
    copy(targets[overlay_count++], target, PATH_SIZE);
    /* Persist ownership before mounting, so process replacement can recover. */
    if (!save_journal()) {
        --overlay_count;
        return -1;
    }
    if (mount(resolved, target, (const char *)0, 4096, (const void *)0)) {
        --overlay_count;
        if (!save_journal())
            poisoned = 1;
        return -1;
    }
    if (mount((const char *)0, target, (const char *)0, 4096 | 32 | 1, (const void *)0))
        return -1;
    return 0;
}
static int set_startup_activity(const struct dashborne_host *host, const char *name) {
    int owner = 0;
    for (unsigned int i = 0; i < loaded_count; ++i)
        if (host == &loaded[i].host)
            owner = 1;
    if (!owner || !activating || !name || !*name || length(name) >= sizeof(startup_activity) ||
        (startup_owner && startup_owner != host))
        return -1;
    for (const char *p = name; *p; ++p)
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') ||
              *p == '_'))
            return -1;
    copy(startup_activity, name, sizeof(startup_activity));
    startup_owner = host;
    return 0;
}
const char *extensions_activity(const char *official_activity) {
    return *startup_activity ? startup_activity : official_activity;
}
static int library_path(const char *id, char *out) {
    if (!valid_id(id) || equal(id,"extra-menu") || equal(id,"widget-runtime"))
        return 0;
    unsigned int at = length(DBX_ROOT "/extensions/");
    if (!copy(out, DBX_ROOT "/extensions/", PATH_SIZE))
        return 0;
    copy(out + at, id, PATH_SIZE - at);
    at += length(id);
    return copy(out + at, "/libextension.so", PATH_SIZE - at);
}
int extension_probe(const char *id) {
    char path[PATH_SIZE];
    if (!library_path(id, path))
        return 0;
    int fd = open(path, 0);
    if (fd < 0)
        return 0;
    unsigned char header[52];
    unsigned int at = 0;
    while (at < sizeof(header)) {
        int n = read(fd, header + at, sizeof(header) - at);
        if (n <= 0)
            break;
        at += (unsigned int)n;
    }
    close(fd);
#ifdef DBX_HOST_TEST
    return at == sizeof(header);
#else
    return at == sizeof(header) && header[0] == 127 && header[1] == 'E' && header[2] == 'L' &&
           header[3] == 'F' && header[4] == 1 && header[5] == 1 && header[6] == 1 &&
           header[16] == 3 && !header[17] && header[18] == 40 && !header[19];
#endif
}
int extensions_stop(void) {
    activating = 0;
    startup_activity[0] = 0;
    startup_owner = (void *)0;
    int ok = 1;
    while (loaded_count) {
        struct loaded_extension *entry = &loaded[loaded_count - 1];
        if (entry->entered && entry->descriptor->deactivate()) {
            poisoned = 1;
            event("extensions: module cleanup failed; reboot required\n");
            ok = 0;
            break;
        }
        retire_activities(&entry->host);
        retire_extra(&entry->host);
        if (entry->library)
            dlclose(entry->library);
        --loaded_count;
    }
    if (!remove_overlays())
        ok = 0;
    return ok && !poisoned;
}
int extensions_start(const char *const *ids, unsigned int count, void *official, void *runtime,
                     const char *protected_target, void (*logger)(const char *)) {
    log_line = logger;
    protected_file = protected_target;
    if (poisoned)
        return -2;
    if (loaded_count || count > MAX_EXTENSIONS-CORE_COUNT || (count && !ids))
        return -1;
    for (unsigned int i = 0; i < count+CORE_COUNT; ++i) {
        struct loaded_extension *e = &loaded[loaded_count++];
        *e = (struct loaded_extension){0};
        if(i<CORE_COUNT) {
            e->descriptor=&core_services[i];
            copy(e->directory,DBX_ROOT,sizeof(e->directory));
        } else {
            unsigned int user_index=i-CORE_COUNT;
            const char *id=ids[user_index];
            char path[PATH_SIZE];
            if (!library_path(id, path) || !extension_probe(id))
                goto failure;
            for (unsigned int j = 0; j < user_index; ++j)
                if (equal(id, ids[j]))
                    goto failure;
            char directory[PATH_SIZE], resolved[4096];
            copy(directory, path, PATH_SIZE);
            directory[length(directory) - length("/libextension.so")] = 0;
            if (!realpath(directory, resolved) || !copy(e->directory, resolved, sizeof(e->directory)))
                goto failure;
            e->library = dlopen(path, 2);
            if (!e->library) {
                const char *reason=dlerror();
                event("extensions: could not load ");event(id);event(": ");
                event(reason ? reason : "no dynamic loader error");event("\n");
                goto failure;
            }
            const struct dashborne_extension *(*get)(void) = (void *)0;
            *(void **)(&get) = dlsym(e->library, "dashborne_extension_v1");
            if (!get)
                goto failure;
            e->descriptor = get();
            const struct dashborne_extension *d = e->descriptor;
            if (!d || d->abi_version != DASHBORNE_EXTENSION_ABI || d->struct_size < sizeof(*d) ||
                !d->id || !equal(d->id, id) || !d->activate || !d->deactivate)
                goto failure;
        }
        const struct dashborne_extension *d=e->descriptor;
        e->host = (struct dashborne_host){DASHBORNE_EXTENSION_ABI,
                                          sizeof(e->host),
                                          e->directory,
                                          official,
                                          runtime,
                                          plugin_log,
                                          dlsym,
                                          overlay_file,
                                          set_startup_activity,
                                          register_plugin_activity,
                                          owns_callback,
                                          register_extra,extra_count,extra_entry_at,open_extra};
        e->entered = 1;
        activating = 1;
        int rc = d->activate(&e->host);
        activating = 0;
        if (rc || poisoned) {
            event("extensions: activation rejected by ");event(d->id);event("\n");
            goto failure;
        }
        event(i<CORE_COUNT?"core: started ":"extensions: activated ");
        event(d->id);
        event("\n");
    }
    return (int)count;
failure:
    event("extensions: activation failed; rolling back\n");
    return extensions_stop() ? -1 : -2;
}
