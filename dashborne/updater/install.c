#include "updater.h"
#include <errno.h>
#include <mbedtls/base64.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>

static void location(char out[PATH_CAP], const char *name) { path_join(out, work(), name); }
cJSON *read_manifest(const char *dir) {
    char p[PATH_CAP];
    path_join(p, dir, "update-manifest.json");
    cJSON *m = read_json(p, 2 * MIB);
    cJSON *files = cJSON_GetObjectItemCaseSensitive(m, "files");
    if (json_integer(m, "format") != 1 || !version_valid(json_string(m, "version")) ||
        !version_valid(json_string(m, "stock_os")) || !cJSON_IsObject(files) ||
        cJSON_GetArraySize(files) < 1 || cJSON_GetArraySize(files) > 4096)
        fail("Unsupported update manifest");
    for (cJSON *f = files->child; f; f = f->next) {
        if (!valid_path(f->string) || !cJSON_IsString(f) || !hex_digest(f->valuestring))
            fail("Invalid manifest file");
        for (cJSON *prev = files->child; prev != f; prev = prev->next)
            if (!strcasecmp(prev->string, f->string))
                fail("Case collision in manifest");
    }
    return m;
}
static uint16_t le16(const unsigned char *b) { return b[0] | (uint16_t)b[1] << 8; }
static uint32_t le32(const unsigned char *b) { return le16(b) | (uint32_t)le16(b + 2) << 16; }
static void zip_read(FILE *f, void *b, size_t n) {
    if (fread(b, 1, n, f) != n)
        fail("Truncated archive");
}
static void seek(FILE *f, uint64_t offset) {
    if (offset > 256 * MIB || fseeko(f, (off_t)offset, SEEK_SET))
        fail("Invalid archive offset");
}
struct entry {
    char *name;
    uint32_t offset, size, packed, crc;
    uint16_t method, flags;
    int directory;
};
static void unpack(FILE *zip, const struct entry *e, const char *dst, uint32_t central) {
    unsigned char header[30];
    seek(zip, e->offset);
    zip_read(zip, header, sizeof(header));
    if (le32(header) != 0x04034b50 || le16(header + 6) != e->flags ||
        le16(header + 8) != e->method || le16(header + 26) != strlen(e->name))
        fail("Inconsistent ZIP header");
    unsigned name_len = le16(header + 26), extra = le16(header + 28);
    char name[1025];
    zip_read(zip, name, name_len);
    name[name_len] = 0;
    if (strcmp(name, e->name))
        fail("Inconsistent ZIP filename");
    if ((uint64_t)e->offset + 30 + name_len + extra + e->packed > central)
        fail("ZIP data overlaps directory");
    seek(zip, (uint64_t)e->offset + 30 + name_len + extra);
    char parent[PATH_CAP];
    snprintf(parent, sizeof(parent), "%s", dst);
    char *slash = strrchr(parent, '/');
    if (!slash)
        fail("Invalid output path");
    *slash = 0;
    mkdirs(parent);
    FILE *out = fopen(dst, "wx");
    if (!out)
        fail("Duplicate or conflicting archive path");
    unsigned char input[32768], output[32768];
    uint64_t count = 0;
    uint32_t left = e->packed;
    uLong crc = crc32(0, NULL, 0);
    z_stream z = {0};
    int result = Z_OK;
    if (e->method == 8 && inflateInit2(&z, -MAX_WBITS) != Z_OK)
        fail("Cannot initialize ZIP decoder");
    while (left) {
        size_t n = left < sizeof(input) ? left : sizeof(input);
        zip_read(zip, input, n);
        left -= n;
        if (!e->method) {
            count += n;
            if (count > e->size || fwrite(input, 1, n, out) != n)
                fail("Invalid or unwritable ZIP data");
            crc = crc32(crc, input, n);
        } else {
            if (result == Z_STREAM_END)
                fail("Trailing compressed data");
            z.next_in = input;
            z.avail_in = (uInt)n;
            do {
                z.next_out = output;
                z.avail_out = sizeof(output);
                result = inflate(&z, Z_NO_FLUSH);
                if (result == Z_BUF_ERROR && !z.avail_in)
                    break; /* Refill at an exact input/output buffer boundary. */
                if (result != Z_OK && result != Z_STREAM_END)
                    fail("Corrupt ZIP stream");
                size_t produced = sizeof(output) - z.avail_out;
                count += produced;
                if (count > e->size || fwrite(output, 1, produced, out) != produced)
                    fail("Invalid or unwritable ZIP data");
                crc = crc32(crc, output, produced);
                if (result == Z_STREAM_END) {
                    if (z.avail_in || left)
                        fail("Trailing compressed data");
                    break;
                }
            } while (z.avail_in || !z.avail_out);
        }
    }
    if (e->method == 8) {
        inflateEnd(&z);
        if (result != Z_STREAM_END)
            fail("Incomplete ZIP stream");
    }
    if (count != e->size || crc != e->crc)
        fail("ZIP size or CRC mismatch");
    if (fflush(out) || fsync(fileno(out)) || fclose(out))
        fail("Cannot sync extracted file");
    if (chmod(dst, 0755))
        fail("Cannot set extracted file mode");
}
cJSON *extract_release(const char *archive, const char *dir, const char *version,
                       const char *stock) {
    FILE *z = fopen(archive, "rb");
    if (!z || fseeko(z, 0, SEEK_END))
        fail("Cannot open release archive");
    off_t length = ftello(z);
    if (length < 22 || length > (off_t)(256 * MIB))
        fail("Invalid archive size");
    size_t tail_size = length < 65557 ? (size_t)length : 65557;
    unsigned char *tail = allocate(tail_size);
    seek(z, length - tail_size);
    zip_read(z, tail, tail_size);
    unsigned char *end = NULL;
    for (size_t k = tail_size - 22 + 1; k-- > 0;)
        if (le32(tail + k) == 0x06054b50 && k + 22 + le16(tail + k + 20) == tail_size) {
            end = tail + k;
            break;
        }
    if (!end || le16(end + 4) || le16(end + 6) || le16(end + 8) != le16(end + 10))
        fail("Unsupported ZIP directory");
    unsigned count = le16(end + 10);
    uint32_t central_size = le32(end + 12), central = le32(end + 16);
    if (!count || count > 5000 ||
        (uint64_t)central + central_size != (uint64_t)(length - tail_size + (end - tail)))
        fail("Invalid ZIP directory bounds");
    free(tail);
    struct entry *entries = allocate(count * sizeof(*entries));
    seek(z, central);
    uint64_t total = 0;
    for (unsigned i = 0; i < count; i++) {
        unsigned char h[46];
        zip_read(z, h, sizeof(h));
        struct entry *e = &entries[i];
        if (le32(h) != 0x02014b50 || le16(h + 34))
            fail("Invalid ZIP entry");
        unsigned n = le16(h + 28), extra = le16(h + 30), comment = le16(h + 32);
        if (!n || n > 1024)
            fail("Invalid ZIP path length");
        e->name = allocate(n + 1);
        zip_read(z, e->name, n);
        if (strlen(e->name) != n)
            fail("NUL in archive path");
        e->directory = e->name[n - 1] == '/';
        if (e->directory)
            e->name[n - 1] = 0;
        if (!valid_path(e->name))
            fail("Unsafe archive entry");
        for (unsigned j = 0; j < i; j++) {
            size_t a = strlen(e->name), b = strlen(entries[j].name);
            if (entries[j].directory)
                b--;
            /* Check implicit parent spelling too: FAT paths are case-insensitive. */
            size_t common = a < b ? a : b;
            if (!strncasecmp(e->name, entries[j].name, common)) {
                if (a == b || (a < b && entries[j].name[a] == '/' && !e->directory) ||
                    (b < a && e->name[b] == '/' && !entries[j].directory))
                    fail("Conflicting archive paths");
            }
            for (size_t k = 0; k < common; k++)
                if (e->name[k] == '/' && entries[j].name[k] == '/' &&
                    !strncasecmp(e->name, entries[j].name, k) &&
                    strncmp(e->name, entries[j].name, k))
                    fail("Case collision in archive");
        }
        if (e->directory)
            e->name[n - 1] = '/';
        uint32_t mode = le32(h + 38) >> 16;
        if ((mode & 0170000) && (mode & 0170000) != (e->directory ? 0040000 : 0100000))
            fail("Non-regular ZIP entry");
        e->flags = le16(h + 8);
        e->method = le16(h + 10);
        e->crc = le32(h + 16);
        e->packed = le32(h + 20);
        e->size = le32(h + 24);
        e->offset = le32(h + 42);
        if ((e->flags & ~(8 | 0x800 | 6)) || (e->method != 0 && e->method != 8) ||
            e->offset >= central || e->size > 128 * MIB || e->packed > 256 * MIB)
            fail("Unsupported ZIP entry");
        total += e->size;
        if (total > 512 * MIB)
            fail("Archive is too large");
        if (fseeko(z, extra + comment, SEEK_CUR))
            fail("Truncated ZIP directory");
    }
    if ((uint64_t)ftello(z) != (uint64_t)central + central_size)
        fail("Inconsistent ZIP directory size");
    mkdirs(dir);
    unsigned extracted = 0;
    for (unsigned i = 0; i < count; i++) {
        struct entry *e = &entries[i];
        if (e->directory || strncmp(e->name, "card/Dashborne/", 15))
            continue;
        char to[PATH_CAP], message[80];
        path_join(to, dir, e->name + 15);
        unpack(z, e, to, central);
        extracted++;
        if (!(i % 32)) {
            snprintf(message, sizeof(message), "Extracting release: %u%%", i * 100 / count);
            status(message);
        }
    }
    fclose(z);
    status("Verifying extracted files...");
    cJSON *m = read_manifest(dir), *files = cJSON_GetObjectItemCaseSensitive(m, "files");
    if (strcmp(json_string(m, "version"), version) || strcmp(json_string(m, "stock_os"), stock))
        fail("Release version or stock OS target mismatch");
    if (extracted != (unsigned)cJSON_GetArraySize(files) + 1)
        fail("Incomplete or unlisted release files");
    for (cJSON *f = files->child; f; f = f->next) {
        char p[PATH_CAP], got[65];
        path_join(p, dir, f->string);
        sha_file(p, got);
        if (strcmp(got, f->valuestring))
            fail("Release file checksum mismatch");
    }
    const char *required[] = {"lib/libdashborne.so", "ui/loader.ftu", "bin/updater",
                              "lib/update-recovery.so", "patches/catalog.tsv"};
    for (unsigned i = 0; i < sizeof(required) / sizeof(*required); i++)
        if (!cJSON_GetObjectItemCaseSensitive(files, required[i]))
            fail("Missing updater component: %s", required[i]);
    char p[PATH_CAP];
    path_join(p, dir, required[0]);
    FILE *elf = fopen(p, "rb");
    unsigned char head[52];
    if (!elf || fread(head, 1, sizeof(head), elf) != sizeof(head) ||
        memcmp(head, "\177ELF\1\1", 6) || le16(head + 18) != 40)
        fail("Release is not an ARM Dashborne build");
    fclose(elf);
    for (unsigned i = 0; i < count; i++)
        free(entries[i].name);
    free(entries);
    return m;
}
struct preserve_context {
    cJSON *old_files, *new_files, *owned;
    const char *stage;
};
static void managed(cJSON *files, cJSON *owned) {
    for (cJSON *f = files ? files->child : NULL; f; f = f->next) {
        if (strncmp(f->string, "extensions/", 11))
            continue;
        const char *end = strchr(f->string + 11, '/');
        if (!end)
            continue;
        char id[256];
        size_t n = end - f->string - 11;
        if (!n || n >= sizeof(id))
            fail("Invalid extension path");
        memcpy(id, f->string + 11, n);
        id[n] = 0;
        if (!cJSON_GetObjectItemCaseSensitive(owned, id))
            cJSON_AddBoolToObject(owned, id, 1);
    }
}
static int skip_preserved(const char *p, void *data) {
    struct preserve_context *c = data;
    if (!*p)
        return 0;
    char first[256];
    size_t n = strcspn(p, "/");
    if (n >= sizeof(first))
        fail("Path component too long");
    memcpy(first, p, n);
    first[n] = 0;
    if (!strcmp(first, "lib") || !strcmp(first, "bin") || !strcmp(first, "ui") ||
        !strcmp(first, "assets") || !strcmp(p, "licenses/updater"))
        return 1;
    if (!strcmp(first, "extensions")) {
        if (!p[n])
            return 0;
        const char *id = p + n + 1;
        size_t k = strcspn(id, "/");
        char key[256];
        if (k >= sizeof(key))
            fail("Extension ID too long");
        memcpy(key, id, k);
        key[k] = 0;
        return cJSON_GetObjectItemCaseSensitive(c->owned, key) != NULL;
    }
    if (!strcmp(first, "patches"))
        return !strcmp(p, "patches/catalog.tsv") ||
               cJSON_GetObjectItemCaseSensitive(c->new_files, p) ||
               cJSON_GetObjectItemCaseSensitive(c->old_files, p);
    if (!strcmp(first, "update-manifest.json") || !strcmp(first, "config.once") ||
        !strcmp(first, "stock.once") || !strcmp(first, "settings.conf") ||
        !strcmp(first, "wifi.conf"))
        return 1;
    char dst[PATH_CAP];
    path_join(dst, c->stage, p);
    return !p[n] && strcmp(first, "licenses") && exists(dst);
}
void preserve(const char *stage, cJSON *manifest) {
    status("Preserving settings and community files...");
    char p[PATH_CAP];
    path_join(p, stage, "previous.cfg");
    if (exists(p) && unlink(p))
        fail("Cannot replace startup backup");
    path_join(p, active(), "update-manifest.json");
    cJSON *old = exists(p) ? read_manifest(active()) : cJSON_CreateObject();
    struct preserve_context c = {cJSON_GetObjectItemCaseSensitive(old, "files"),
                                 cJSON_GetObjectItemCaseSensitive(manifest, "files"),
                                 cJSON_CreateObject(), stage};
    managed(c.old_files, c.owned);
    managed(c.new_files, c.owned);
    copy_tree(active(), stage, skip_preserved, &c);
    char catalog[PATH_CAP];
    path_join(catalog, stage, "patches/catalog.tsv");
    size_t fresh_size;
    char *fresh = read_file(catalog, 2 * MIB, &fresh_size);
    path_join(p, active(), "patches/catalog.tsv");
    char *previous = exists(p) ? read_file(p, 2 * MIB, NULL) : allocate(1);
    cJSON *ids = cJSON_CreateObject();
    char *scan = strdup(fresh), *save = NULL;
    for (char *l = strtok_r(scan, "\n", &save); l; l = strtok_r(NULL, "\n", &save)) {
        char *tab = strchr(l, '\t');
        if (tab)
            *tab = 0;
        if (*l && !cJSON_GetObjectItemCaseSensitive(ids, l))
            cJSON_AddBoolToObject(ids, l, 1);
    }
    free(scan);
    size_t capacity = fresh_size + strlen(previous) + 2;
    char *merged = allocate(capacity);
    memcpy(merged, fresh, fresh_size);
    size_t used = fresh_size;
    if (used && merged[used - 1] != '\n')
        merged[used++] = '\n';
    save = NULL;
    for (char *l = strtok_r(previous, "\n", &save); l; l = strtok_r(NULL, "\n", &save)) {
        char *tab = strchr(l, '\t');
        if (!tab || !strchr(tab + 1, '\t'))
            continue;
        *tab = 0;
        int keep = !cJSON_GetObjectItemCaseSensitive(ids, l) &&
                   !cJSON_GetObjectItemCaseSensitive(c.owned, l);
        if (keep)
            cJSON_AddBoolToObject(ids, l, 1);
        *tab = '\t';
        if (keep) {
            size_t n = strlen(l);
            memcpy(merged + used, l, n);
            used += n;
            merged[used++] = '\n';
        }
    }
    atomic_write(catalog, merged, used, 0644);
    free(merged);
    free(fresh);
    free(previous);
    cJSON_Delete(ids);
    cJSON_Delete(c.owned);
    cJSON_Delete(old);
}
void transaction_step(const char *step) {
#ifdef UPDATER_TEST
    const char *fault = getenv("UPDATER_TEST_FAULT");
    if (fault && !strcmp(fault, step))
        _exit(90);
#else
    (void)step;
#endif
}
static void save_journal(cJSON *j) {
    char p[PATH_CAP];
    location(p, "journal.json");
    write_json(p, j, 0600);
}
static void phase(cJSON *j, const char *value) {
    if (!cJSON_ReplaceItemInObjectCaseSensitive(j, "Phase", cJSON_CreateString(value)))
        fail("Invalid transaction journal");
    save_journal(j);
}
static cJSON *load_journal(void) {
    char p[PATH_CAP];
    location(p, "journal.json");
    if (!exists(p))
        return NULL;
    return read_json(p, 128 * 1024);
}
static void restore_startup(cJSON *j) {
    const char *encoded = json_string(j, "Startup");
    size_t size = 0;
    unsigned char out[65536];
    if (!*encoded || mbedtls_base64_decode(out, sizeof(out), &size, (const unsigned char *)encoded,
                                           strlen(encoded)))
        fail("Invalid startup backup");
    char p[PATH_CAP];
    path_join(p, card, "EasyUI.cfg");
    atomic_write(p, out, size, 0644);
}
static void finish_journal(void) {
    char p[PATH_CAP];
    location(p, "journal.json");
    if (unlink(p))
        fail("Cannot clear recovery journal");
    sync_dir(work());
}
void apply(const char *stage) {
    char p[PATH_CAP], dst[PATH_CAP];
    location(p, "journal.json");
    if (exists(p))
        fail("An unfinished update must be recovered first");
    path_join(p, card, "EasyUI.cfg");
    size_t size;
    char *startup = read_file(p, 65536, &size);
    cJSON *cfg = parse_json(startup);
    if (strcmp(json_string(cfg, "startupLibPath"), "/mnt/extsd/Dashborne/lib/libdashborne.so"))
        fail("Dashborne is not the selected startup app");
    const char *sources[] = {"bin/updater", "lib/update-recovery.so"},
               *targets[] = {"updater", "recovery.so"};
    for (int i = 0; i < 2; i++) {
        path_join(p, active(), sources[i]);
        size_t n;
        char *b = read_file(p, 32 * MIB, &n);
        location(dst, targets[i]);
        atomic_write(dst, b, n, 0755);
        free(b);
    }
    path_join(p, stage, "config.once");
    atomic_write(p, "", 0, 0644);
    status("Finishing microSD writes...");
    sync_tree(stage);
    status("Removing older backup...");
    location(p, "previous");
    remove_tree(p);
    status("Activating update...");
    unsigned char *encoded = allocate(size * 2 + 8);
    size_t n;
    if (mbedtls_base64_encode(encoded, size * 2 + 8, &n, (unsigned char *)startup, size))
        fail("Cannot encode startup backup");
    cJSON *j = cJSON_CreateObject();
    cJSON_AddStringToObject(j, "Phase", "armed");
    cJSON_AddStringToObject(j, "Startup", (char *)encoded);
    free(encoded);
    free(startup);
    save_journal(j);
    transaction_active = 1;
    cJSON_ReplaceItemInObjectCaseSensitive(
        cfg, "startupLibPath", cJSON_CreateString("/mnt/extsd/.dashborne-update/recovery.so"));
    path_join(p, card, "EasyUI.cfg");
    write_json(p, cfg, 0644);
    cJSON_Delete(cfg);
    transaction_step("armed");
    location(p, "previous");
    if (rename(active(), p))
        fail("Cannot back up current installation");
    sync_dir(card);
    sync_dir(work());
    transaction_step("old-moved");
    if (rename(stage, active()))
        fail("Cannot activate staged installation");
    sync_dir(card);
    sync_dir(work());
    transaction_step("new-moved");
    phase(j, "installed");
    transaction_step("installed");
    cJSON_Delete(j);
    transaction_active = 0;
}
void recover_install(void) {
    cJSON *j = load_journal();
    if (!j)
        return;
    const char *s = json_string(j, "Phase");
    if (!strcmp(s, "installed")) {
        phase(j, "booting");
        cJSON_Delete(j);
        return;
    }
    if (strcmp(s, "armed") && strcmp(s, "booting") && strcmp(s, "restoring"))
        fail("Unknown recovery state");
    char p[PATH_CAP];
    location(p, "previous");
    if (exists(p)) {
        phase(j, "restoring");
        remove_tree(active());
        if (rename(p, active()))
            fail("Cannot restore previous installation");
        sync_dir(card);
        sync_dir(work());
    }
    path_join(p, active(), "lib/libdashborne.so");
    if (!exists(p))
        fail("Previous installation is unavailable");
    restore_startup(j);
    path_join(p, active(), "config.once");
    atomic_write(p, "", 0, 0644);
    finish_journal();
    cJSON_Delete(j);
}
void confirm_install(void) {
    cJSON *j = load_journal();
    if (!j)
        return;
    if (strcmp(json_string(j, "Phase"), "booting"))
        fail("Update has not booted");
    restore_startup(j);
    finish_journal();
    cJSON_Delete(j);
}
void rollback_install(void) {
    char previous[PATH_CAP], stage[PATH_CAP];
    location(previous, "previous");
    cJSON *m = read_manifest(previous);
    if (strcmp(installed_os(), json_string(m, "stock_os")))
        fail("Previous installation targets another stock OS");
    space_check(768 * MIB);
    location(stage, "stage");
    remove_tree(stage);
    status("Preparing previous installation...");
    copy_tree(previous, stage, NULL, NULL);
    preserve(stage, m);
    cJSON_Delete(m);
    apply(stage);
}
