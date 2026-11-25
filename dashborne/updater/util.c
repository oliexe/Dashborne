#include "updater.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <mbedtls/sha256.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <zlib.h>

const char *card = "/mnt/extsd";
const char *status_path = "/tmp/dashborne-update.status";
const char *list_path = "/tmp/dashborne-releases.tsv";
const char *cache_path = "/tmp/dashborne-releases.json";
jmp_buf failure;
char error_text[512];
int transaction_active;
_Noreturn void fail(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(error_text, sizeof(error_text), fmt, ap);
    va_end(ap);
    longjmp(failure, 1);
}
void *allocate(size_t n) {
    void *p = calloc(n ? n : 1, 1);
    if (!p)
        fail("Not enough memory");
    return p;
}
void path_join(char out[PATH_CAP], const char *a, const char *b) {
    if (snprintf(out, PATH_CAP, "%s/%s", a, b) >= PATH_CAP)
        fail("Path too long");
}
const char *work(void) {
    static char p[PATH_CAP];
    path_join(p, card, ".dashborne-update");
    return p;
}
const char *active(void) {
    static char p[PATH_CAP];
    path_join(p, card, "Dashborne");
    return p;
}
int exists(const char *p) {
    struct stat s;
    if (!lstat(p, &s))
        return 1;
    if (errno != ENOENT)
        fail("Cannot inspect %s", p);
    return 0;
}
static void directory(const char *p) {
    struct stat s;
    if (mkdir(p, 0755) && errno != EEXIST)
        fail("Cannot create directory: %s", strerror(errno));
    if (lstat(p, &s) || !S_ISDIR(s.st_mode))
        fail("Not a regular directory: %s", p);
}
void mkdirs(const char *p) {
    char b[PATH_CAP];
    if (strlen(p) >= sizeof(b))
        fail("Path too long");
    strcpy(b, p);
    for (char *c = b + 1; *c; c++)
        if (*c == '/') {
            *c = 0;
            directory(b);
            *c = '/';
        }
    directory(b);
}
static void parent(char out[PATH_CAP], const char *p) {
    if (strlen(p) >= PATH_CAP)
        fail("Path too long");
    strcpy(out, p);
    char *slash = strrchr(out, '/');
    if (!slash)
        strcpy(out, ".");
    else if (slash == out)
        slash[1] = 0;
    else
        *slash = 0;
}
void sync_dir(const char *p) {
    int fd = open(p, O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
    if (fd < 0)
        fail("Cannot open directory");
    int rc = fsync(fd), saved = errno;
    close(fd);
    if (rc)
        fail("Cannot sync directory: %s", strerror(saved));
}
static void write_all(int fd, const void *data, size_t n) {
    const char *b = data;
    while (n) {
        ssize_t k = write(fd, b, n);
        if (k < 0 && errno == EINTR)
            continue;
        if (k <= 0)
            fail("Cannot write update: %s", strerror(errno));
        b += k;
        n -= (size_t)k;
    }
}
void atomic_write(const char *p, const void *b, size_t n, mode_t mode) {
    char dir[PATH_CAP], temp[PATH_CAP];
    parent(dir, p);
    mkdirs(dir);
    if (snprintf(temp, sizeof(temp), "%s.next", p) >= PATH_CAP)
        fail("Path too long");
    int fd = open(temp, O_CREAT | O_TRUNC | O_WRONLY | O_NOFOLLOW, mode);
    if (fd < 0)
        fail("Cannot write update file");
    write_all(fd, b, n);
    int rc = fsync(fd);
    int ce = close(fd);
    if (rc || ce || rename(temp, p))
        fail("Cannot commit update file");
    sync_dir(dir);
}
void clean_text(char *out, size_t capacity, const char *s) {
    size_t i = 0;
    if (!capacity)
        return;
    while (*s && i + 1 < capacity) {
        unsigned char c = (unsigned char)*s;
        size_t n = c < 128                  ? 1
                   : c >= 0xc2 && c <= 0xdf ? 2
                   : c >= 0xe0 && c <= 0xef ? 3
                   : c >= 0xf0 && c <= 0xf4 ? 4
                                            : 0;
        if (!n) {
            s++;
            continue;
        }
        int valid = 1;
        for (size_t k = 1; k < n; k++)
            if (!s[k] || ((unsigned char)s[k] & 0xc0) != 0x80) {
                valid = 0;
                break;
            }
        if (n > 1 && ((c == 0xe0 && (unsigned char)s[1] < 0xa0) ||
                      (c == 0xed && (unsigned char)s[1] >= 0xa0) ||
                      (c == 0xf0 && (unsigned char)s[1] < 0x90) ||
                      (c == 0xf4 && (unsigned char)s[1] >= 0x90)))
            valid = 0;
        if (!valid) {
            s++;
            continue;
        }
        if (i + n >= capacity)
            break;
        if (n == 1)
            out[i++] = c < 32 || c == 127 ? ' ' : *s;
        else {
            memcpy(out + i, s, n);
            i += n;
        }
        s += n;
    }
    out[i] = 0;
}
void status(const char *text) {
    /* Status is advisory and must never interrupt transaction recovery. */
    char b[241], temp[PATH_CAP];
    clean_text(b, sizeof(b), text);
    if (snprintf(temp, sizeof(temp), "%s.next", status_path) >= PATH_CAP)
        return;
    int fd = open(temp, O_CREAT | O_TRUNC | O_WRONLY | O_NOFOLLOW, 0600);
    if (fd >= 0) {
        size_t n = strlen(b);
        ssize_t k = write(fd, b, n);
        if (k == (ssize_t)n && !fsync(fd)) {
            close(fd);
            (void)rename(temp, status_path);
        } else
            close(fd);
    }
}
char *read_file(const char *p, size_t max, size_t *size) {
    int fd = open(p, O_RDONLY | O_NOFOLLOW);
    struct stat s;
    if (fd < 0 || fstat(fd, &s) || !S_ISREG(s.st_mode) || s.st_size < 0 ||
        (uint64_t)s.st_size > max)
        fail("Cannot read file or size limit exceeded: %s", p);
    size_t n = (size_t)s.st_size, got = 0;
    char *b = allocate(n + 1);
    while (got < n) {
        ssize_t k = read(fd, b + got, n - got);
        if (k < 0 && errno == EINTR)
            continue;
        if (k <= 0)
            fail("Truncated file");
        got += k;
    }
    close(fd);
    if (size)
        *size = n;
    return b;
}
void remove_tree(const char *p) {
    struct stat s;
    if (lstat(p, &s)) {
        if (errno == ENOENT)
            return;
        fail("Cannot inspect removal path");
    }
    if (!S_ISDIR(s.st_mode)) {
        if (unlink(p))
            fail("Cannot remove file");
        return;
    }
    DIR *d = opendir(p);
    if (!d)
        fail("Cannot read directory");
    struct dirent *e;
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
            continue;
        char child[PATH_CAP];
        path_join(child, p, e->d_name);
        remove_tree(child);
    }
    closedir(d);
    if (rmdir(p))
        fail("Cannot remove directory");
}
void copy_file(const char *src, const char *dst) {
    char dir[PATH_CAP];
    parent(dir, dst);
    mkdirs(dir);
    int in = open(src, O_RDONLY | O_NOFOLLOW);
    struct stat s;
    if (in < 0 || fstat(in, &s) || !S_ISREG(s.st_mode))
        fail("Cannot copy non-regular file");
    int out = open(dst, O_CREAT | O_EXCL | O_WRONLY | O_NOFOLLOW, s.st_mode & 0777);
    if (out < 0)
        fail("Cannot create copied file");
    char b[32768];
    ssize_t n;
    while ((n = read(in, b, sizeof(b))) != 0) {
        if (n < 0 && errno == EINTR)
            continue;
        if (n < 0)
            fail("Cannot read copied file");
        write_all(out, b, n);
    }
    close(in);
    int rc = fsync(out), ce = close(out);
    if (rc || ce)
        fail("Cannot sync copied file");
}
static void walk_copy(const char *src, const char *dst, const char *relative, skip_fn skip,
                      void *context, unsigned depth) {
    if (depth > 64)
        fail("Installation directory is too deep");
    char from[PATH_CAP], to[PATH_CAP];
    path_join(from, src, relative);
    path_join(to, dst, relative);
    struct stat s;
    if (lstat(from, &s))
        fail("Cannot inspect source");
    if (skip && skip(relative, context))
        return;
    if (S_ISDIR(s.st_mode)) {
        mkdirs(to);
        DIR *d = opendir(from);
        if (!d)
            fail("Cannot read installation");
        struct dirent *e;
        while ((e = readdir(d))) {
            if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
                continue;
            char next[PATH_CAP];
            if (*relative)
                path_join(next, relative, e->d_name);
            else
                snprintf(next, sizeof(next), "%s", e->d_name);
            walk_copy(src, dst, next, skip, context, depth + 1);
        }
        closedir(d);
    } else {
        if (!S_ISREG(s.st_mode))
            fail("Non-regular file in installation");
        if (exists(to)) {
            if (lstat(to, &s) || !S_ISREG(s.st_mode) || unlink(to))
                fail("Cannot replace staged file");
        }
        copy_file(from, to);
    }
}
void copy_tree(const char *src, const char *dst, skip_fn skip, void *context) {
    walk_copy(src, dst, "", skip, context, 0);
}
static void walk_sync(const char *p, unsigned depth) {
    if (depth > 64)
        fail("Installation directory is too deep");
    DIR *d = opendir(p);
    if (!d)
        fail("Cannot sync installation");
    struct dirent *e;
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
            continue;
        char child[PATH_CAP];
        path_join(child, p, e->d_name);
        struct stat s;
        if (lstat(child, &s))
            fail("Cannot inspect staged file");
        if (S_ISDIR(s.st_mode))
            walk_sync(child, depth + 1);
        else if (!S_ISREG(s.st_mode))
            fail("Non-regular staged file");
    }
    closedir(d);
    sync_dir(p);
}
void sync_tree(const char *p) { walk_sync(p, 0); }
void sha_file(const char *p, char out[65]) {
    int fd = open(p, O_RDONLY | O_NOFOLLOW);
    if (fd < 0)
        fail("Cannot hash file");
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    if (mbedtls_sha256_starts(&ctx, 0))
        fail("SHA256 initialization failed");
    unsigned char b[32768], digest[32];
    ssize_t n;
    while ((n = read(fd, b, sizeof(b))) != 0) {
        if (n < 0 && errno == EINTR)
            continue;
        if (n < 0 || mbedtls_sha256_update(&ctx, b, n))
            fail("SHA256 read failed");
    }
    close(fd);
    if (mbedtls_sha256_finish(&ctx, digest))
        fail("SHA256 failed");
    mbedtls_sha256_free(&ctx);
    for (int i = 0; i < 32; i++)
        snprintf(out + i * 2, 3, "%02x", digest[i]);
}
void space_check(uint64_t bytes) {
    struct statvfs s;
    if (statvfs(card, &s) || (uint64_t)s.f_bavail * s.f_frsize < bytes)
        fail("Not enough free space on microSD");
}
int valid_path(const char *p) {
    if (!*p || *p == '/' || strlen(p) >= 1024)
        return 0;
    const char *start = p;
    for (const char *c = p;; c++) {
        if (*c && ((unsigned char)*c < 32 || strchr("\\:*?\"<>|", *c)))
            return 0;
        if (!*c || *c == '/') {
            size_t n = (size_t)(c - start);
            if (!n || n > 255 || c[-1] == '.' || c[-1] == ' ')
                return 0;
            start = c + 1;
        }
        if (!*c)
            return 1;
    }
}
int version_valid(const char *s) {
    for (int i = 0; i < 3; i++) {
        int n = 0;
        while (*s >= '0' && *s <= '9') {
            s++;
            if (++n > 9)
                return 0;
        }
        if (!n || (i < 2 && *s++ != '.'))
            return 0;
    }
    return !*s;
}
int hex_digest(const char *s) {
    if (strlen(s) != 64)
        return 0;
    for (; *s; s++)
        if (!strchr("0123456789abcdef", *s))
            return 0;
    return 1;
}
static void validate_json(cJSON *j, unsigned depth) {
    if (depth > 64)
        fail("JSON nesting limit exceeded");
    for (cJSON *c = j->child; c; c = c->next) {
        if (cJSON_IsObject(j))
            for (cJSON *other = j->child; other != c; other = other->next)
                if (!strcmp(other->string, c->string))
                    fail("Duplicate JSON key");
        validate_json(c, depth + 1);
    }
}
cJSON *parse_json(const char *data) {
    if (strstr(data, "\\u0000"))
        fail("NUL escape in JSON");
    cJSON *j = cJSON_ParseWithOpts(data, NULL, 1);
    if (!j)
        fail("Invalid JSON");
    validate_json(j, 0);
    return j;
}
cJSON *read_json(const char *path, size_t max) {
    size_t n;
    char *b = read_file(path, max, &n);
    if (strlen(b) != n)
        fail("NUL in JSON");
    cJSON *j = parse_json(b);
    free(b);
    return j;
}
void write_json(const char *path, cJSON *j, mode_t mode) {
    char *b = cJSON_PrintUnformatted(j);
    if (!b)
        fail("Cannot encode JSON");
    atomic_write(path, b, strlen(b), mode);
    cJSON_free(b);
}
const char *json_string(const cJSON *j, const char *key) {
    cJSON *s = cJSON_GetObjectItemCaseSensitive(j, key);
    return cJSON_IsString(s) ? s->valuestring : "";
}
long long json_integer(const cJSON *j, const char *key) {
    cJSON *n = cJSON_GetObjectItemCaseSensitive(j, key);
    if (!cJSON_IsNumber(n) || n->valuedouble < 0 || n->valuedouble > 9007199254740991.0 ||
        n->valuedouble != (double)(long long)n->valuedouble)
        return -1;
    return (long long)n->valuedouble;
}
const char *installed_os(void) {
#ifdef UPDATER_TEST
    const char *test = getenv("UPDATER_TEST_OS");
    if (test)
        return test;
#endif
    int fd = open("/res/lib/libzkgui.so", O_RDONLY);
    if (fd < 0)
        fail("Cannot read installed OS version");
    unsigned char b[32768];
    ssize_t n;
    uint64_t total = 0;
    uLong crc = crc32(0, NULL, 0);
    while ((n = read(fd, b, sizeof(b))) > 0) {
        total += n;
        if (total > 32 * MIB)
            break;
        crc = crc32(crc, b, n);
    }
    close(fd);
    if (!n && total == 3464120) {
        if (crc == 0xed0e1aa4)
            return "6.11.4";
        if (crc == 0x9de99542)
            return "6.11.2";
    }
    fail("Installed stock OS is not recognized; check compatibility first");
}
