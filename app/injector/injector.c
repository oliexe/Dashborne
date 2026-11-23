#include "patch.h"
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
extern int access(const char *, int);
extern int mkdir(const char *, unsigned int);
extern int mount(const char *, const char *, const char *, unsigned long, const void *);
#define TARGET "/res/ui/dash1act.ftu"
#define DIRECTORY "/mnt/extsd/Dashborne/injector"
#define CACHE DIRECTORY "/generated.ftu"
#define PENDING CACHE ".next"
#define LIMIT (2u * 1024u * 1024u)
static unsigned char *load(const char *path, unsigned int *size) {
    int fd = open(path, 0);
    if (fd < 0) return (void *)0;
    unsigned char *data = malloc(LIMIT + 1);
    if (!data) { close(fd); return (void *)0; }
    unsigned int used = 0;
    int n;
    while ((n = read(fd, data + used, LIMIT + 1 - used)) > 0) {
        used += (unsigned int)n;
        if (used > LIMIT) break;
    }
    close(fd);
    if (n < 0 || used > LIMIT) { free(data); return (void *)0; }
    *size = used; return data;
}
void injector_apply(void (*log)(const char *)) {
    if (access(DIRECTORY "/disabled", 0) == 0) { log("patch: disabled\n"); return; }
    unsigned int base_size = 0, patch_size = 0, result_size = 0;
    unsigned char *base = load(TARGET, &base_size);
    unsigned char *patch = load(DIRECTORY "/badge.dbp", &patch_size), *result = (void *)0;
    int valid = base && patch && patch_layout(base, base_size, patch, patch_size, &result, &result_size);
    free(base); free(patch);
    if (!valid) { log("patch: skipped, missing or incompatible input\n"); return; }
    (void)mkdir(DIRECTORY, 0755);
    int fd = open(PENDING, 1 | 0100 | 01000, 0644);
    unsigned int done = 0;
    if (fd >= 0) {
        while (done < result_size) {
            int n = write(fd, result + done, result_size - done);
            if (n <= 0) break;
            done += (unsigned int)n;
        }
        if (fsync(fd)) done = 0;
        if (close(fd)) done = 0;
    }
    free(result);
    if (done != result_size || rename(PENDING, CACHE)) {
        (void)unlink(PENDING); log("patch: cache write failed\n"); return;
    }
    if (mount(CACHE, TARGET, (const char *)0, 4096, (const void *)0)) {
        log("patch: bind mount failed\n"); return;
    }
    log("patch: dashborne-badge applied\n");
}
