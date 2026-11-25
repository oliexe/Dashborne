#ifndef DASHBORNE_UPDATER_H
#define DASHBORNE_UPDATER_H
#define _FILE_OFFSET_BITS 64
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#define _DARWIN_C_SOURCE
#include "cJSON.h"
#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#define MIB (1024ULL * 1024)
#define PATH_CAP 4096
extern const char *card, *status_path, *list_path, *cache_path;
extern jmp_buf failure;
extern char error_text[512];
extern int transaction_active;
_Noreturn void fail(const char *fmt, ...);
void *allocate(size_t n);
void path_join(char out[PATH_CAP], const char *a, const char *b);
const char *work(void);
const char *active(void);
int exists(const char *p);
void mkdirs(const char *p);
void sync_dir(const char *p);
void atomic_write(const char *p, const void *b, size_t n, mode_t mode);
void status(const char *text);
char *read_file(const char *p, size_t max, size_t *size);
void remove_tree(const char *p);
void copy_file(const char *src, const char *dst);
typedef int (*skip_fn)(const char *, void *);
void copy_tree(const char *src, const char *dst, skip_fn skip, void *context);
void sync_tree(const char *p);
void sha_file(const char *p, char out[65]);
void space_check(uint64_t bytes);
int valid_path(const char *p);
int version_valid(const char *s);
int hex_digest(const char *s);
void clean_text(char *out, size_t capacity, const char *s);
cJSON *parse_json(const char *data);
cJSON *read_json(const char *path, size_t max);
void write_json(const char *path, cJSON *json, mode_t mode);
const char *json_string(const cJSON *j, const char *key);
long long json_integer(const cJSON *j, const char *key);
const char *installed_os(void);
cJSON *read_manifest(const char *dir);
cJSON *extract_release(const char *zip, const char *dir, const char *version, const char *stock);
void preserve(const char *stage, cJSON *manifest);
void apply(const char *stage);
void recover_install(void);
void confirm_install(void);
void rollback_install(void);
void transaction_step(const char *step);
int allowed_url(const char *url);
void network_init(void);
void list_releases(void);
void download_release(const char *id);
void http_download(const char *url, const char *path, uint64_t limit, int progress);
char *http_text(const char *url, size_t limit);
cJSON *filter_releases(cJSON *releases);
#endif
