#include "roots.h"
#include "updater.h"
#include <curl/curl.h>
#include <errno.h>
#include <fcntl.h>
#include <mbedtls/platform_time.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

static time_t tls_epoch;
static struct timespec tls_start;
static mbedtls_time_t tls_time(mbedtls_time_t *out) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    mbedtls_time_t t = tls_epoch + now.tv_sec - tls_start.tv_sec;
    if (out)
        *out = t;
    return t;
}
int allowed_url(const char *url) {
    if (strncmp(url, "https://", 8))
        return 0;
    const char *slash = strchr(url + 8, '/');
    if (!slash)
        return 0;
    size_t n = slash - url - 8;
    const char *hosts[] = {"api.github.com", "github.com", "release-assets.githubusercontent.com",
                           "objects.githubusercontent.com"};
    int allowed = 0;
    for (unsigned i = 0; i < sizeof(hosts) / sizeof(*hosts); i++)
        if (strlen(hosts[i]) == n && !strncmp(url + 8, hosts[i], n))
            allowed = 1;
    for (const unsigned char *p = (const unsigned char *)url; *p; p++)
        if (*p <= 32 || *p == 127 || *p == '\\' || *p == '#')
            return 0;
    return allowed;
}
static time_t time_estimate(void) {
    int y, m, d;
    if (sscanf(UPDATER_BUILD_DATE, "%d-%d-%d", &y, &m, &d) != 3)
        fail("Invalid updater build date");
    struct tm floor_tm = {.tm_year = y - 1900, .tm_mon = m - 1, .tm_mday = d};
    time_t floor = timegm(&floor_tm), now = time(NULL);
    if (now >= floor)
        return now;
    const char *hosts[] = {"time.cloudflare.com", "time.google.com", "pool.ntp.org"};
    for (unsigned i = 0; i < sizeof(hosts) / sizeof(*hosts); i++) {
        struct addrinfo hints = {.ai_family = AF_INET, .ai_socktype = SOCK_DGRAM},
                        *addresses = NULL;
        if (getaddrinfo(hosts[i], "123", &hints, &addresses))
            continue;
        int fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (fd < 0) {
            freeaddrinfo(addresses);
            continue;
        }
        struct timeval timeout = {.tv_sec = 3};
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        unsigned char request[48] = {0x23}, reply[49];
        int random = open("/dev/urandom", O_RDONLY);
        if (random < 0) {
            close(fd);
            freeaddrinfo(addresses);
            continue;
        }
        ssize_t got = read(random, request + 40, 8);
        close(random);
        int connected = connect(fd, addresses->ai_addr, addresses->ai_addrlen);
        freeaddrinfo(addresses);
        ssize_t n = -1;
        if (got == 8 && !connected && send(fd, request, sizeof(request), 0) == sizeof(request))
            n = recv(fd, reply, sizeof(reply), 0);
        close(fd);
        if (n != 48 || (reply[0] & 7) != 4 || reply[0] >> 6 == 3 || reply[1] < 1 || reply[1] > 15 ||
            memcmp(reply + 24, request + 40, 8))
            continue;
        uint32_t seconds = (uint32_t)reply[40] << 24 | (uint32_t)reply[41] << 16 |
                           (uint32_t)reply[42] << 8 | reply[43];
        /* NTP era rollover in 2036. Choose the era containing the build date. */
        int64_t epoch = (int64_t)seconds - 2208988800LL;
        if (epoch < (int64_t)floor - 2147483648LL)
            epoch += 4294967296LL;
        if (epoch >= floor && epoch <= (int64_t)floor + 10LL * 366 * 86400)
            return (time_t)epoch;
    }
    fail("Cannot obtain time for HTTPS. Check internet access and retry");
}
void network_init(void) {
    tls_epoch = time_estimate();
    if (clock_gettime(CLOCK_MONOTONIC, &tls_start))
        fail("Cannot read monotonic clock");
    if (mbedtls_platform_set_time(tls_time) || curl_global_init(CURL_GLOBAL_DEFAULT))
        fail("Cannot initialize HTTPS");
}
struct response {
    FILE *file;
    char *text;
    size_t length;
    uint64_t limit;
    long code;
    char location[8192];
    int progress;
    time_t last;
    int bad_header;
};
static size_t header_callback(char *b, size_t size, size_t count, void *data) {
    struct response *r = data;
    size_t n = size * count;
    if (n >= 5 && !memcmp(b, "HTTP/", 5)) {
        const char *space = memchr(b, ' ', n);
        r->code = space ? strtol(space + 1, NULL, 10) : 0;
        r->location[0] = 0;
    } else if (n > 9 && !strncasecmp(b, "Location:", 9)) {
        size_t start = 9, end = n;
        while (start < end && (b[start] == ' ' || b[start] == '\t'))
            start++;
        while (end > start && (b[end - 1] == '\r' || b[end - 1] == '\n' || b[end - 1] == ' '))
            end--;
        if (end - start >= sizeof(r->location) || r->location[0])
            r->bad_header = 1;
        else {
            memcpy(r->location, b + start, end - start);
            r->location[end - start] = 0;
        }
    }
    return n;
}
static size_t body_callback(char *b, size_t size, size_t count, void *data) {
    struct response *r = data;
    size_t n = size * count;
    if ((uint64_t)r->length + n > (r->code == 200 ? r->limit : 65536))
        return 0;
    if (r->code == 200) {
        if (r->file) {
            if (fwrite(b, 1, n, r->file) != n)
                return 0;
        } else {
            char *new_text = realloc(r->text, r->length + n + 1);
            if (!new_text)
                return 0;
            r->text = new_text;
            memcpy(r->text + r->length, b, n);
            r->text[r->length + n] = 0;
        }
    }
    r->length += n;
    if (r->progress && r->code == 200 && time(NULL) != r->last) {
        char message[80];
        snprintf(message, sizeof(message), "Downloading release: %llu%%",
                 (unsigned long long)(r->length * 100 / r->limit));
        status(message);
        r->last = time(NULL);
    }
    return n;
}
static char *request(const char *raw, const char *path, uint64_t limit, int progress) {
#ifdef UPDATER_TEST
    const char *fixture = getenv("UPDATER_TEST_HTTP");
    if (fixture) {
        if (!allowed_url(raw))
            fail("Unexpected release URL");
        const char *name = strstr(raw, "checksum")  ? "sum"
                           : strstr(raw, "archive") ? "archive"
                                                    : "releases";
        char p[PATH_CAP];
        path_join(p, fixture, name);
        if (exists(p)) {
            size_t n;
            char *b = read_file(p, limit, &n);
            if (path) {
                atomic_write(path, b, n, 0600);
                free(b);
                return NULL;
            }
            return b;
        }
        fail("GitHub returned HTTP 503; retry later");
    }
#endif
    char url[8192];
    if (strlen(raw) >= sizeof(url))
        fail("Release URL too long");
    strcpy(url, raw);
    for (int hop = 0; hop < 7; hop++) {
        if (!allowed_url(url))
            fail("Unexpected release URL or redirect");
        struct response r = {.limit = limit, .progress = progress};
        if (path) {
            int fd = open(path, O_CREAT | O_TRUNC | O_WRONLY | O_NOFOLLOW, 0600);
            if (fd < 0 || !(r.file = fdopen(fd, "wb")))
                fail("Cannot write download");
        }
        CURL *curl = curl_easy_init();
        if (!curl)
            fail("Cannot initialize HTTPS request");
        struct curl_blob ca = {(void *)roots, sizeof(roots) - 1, CURL_BLOB_NOCOPY};
#define OPTION(key, value)                                                                         \
    do {                                                                                           \
        if (curl_easy_setopt(curl, key, value) != CURLE_OK)                                        \
            fail("HTTPS option unavailable: " #key);                                               \
    } while (0)
        OPTION(CURLOPT_URL, url);
        OPTION(CURLOPT_USERAGENT, "Dashborne-Updater");
        OPTION(CURLOPT_PROXY, "");
        OPTION(CURLOPT_PROTOCOLS_STR, "https");
        OPTION(CURLOPT_FOLLOWLOCATION, 0L);
        OPTION(CURLOPT_SSL_VERIFYPEER, 1L);
        OPTION(CURLOPT_SSL_VERIFYHOST, 2L);
        OPTION(CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
        OPTION(CURLOPT_CAINFO_BLOB, &ca);
        OPTION(CURLOPT_CONNECTTIMEOUT, 12L);
        OPTION(CURLOPT_TIMEOUT, 600L);
        OPTION(CURLOPT_LOW_SPEED_TIME, 30L);
        OPTION(CURLOPT_LOW_SPEED_LIMIT, 128L);
        OPTION(CURLOPT_NOSIGNAL, 1L);
        OPTION(CURLOPT_WRITEFUNCTION, body_callback);
        OPTION(CURLOPT_WRITEDATA, &r);
        OPTION(CURLOPT_HEADERFUNCTION, header_callback);
        OPTION(CURLOPT_HEADERDATA, &r);
        CURLcode result = curl_easy_perform(curl);
        long code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
        curl_easy_cleanup(curl);
        if (r.file) {
            int rc = fflush(r.file);
            if (!rc)
                rc = fsync(fileno(r.file));
            int ce = fclose(r.file);
            if (rc || ce)
                fail("Cannot sync downloaded file");
        }
        if (result != CURLE_OK || r.bad_header) {
            free(r.text);
            fail("HTTPS request failed: %s", curl_easy_strerror(result));
        }
        if (code == 200)
            return r.text ? r.text : allocate(1);
        free(r.text);
        if ((code == 301 || code == 302 || code == 303 || code == 307 || code == 308) &&
            allowed_url(r.location)) {
            strcpy(url, r.location);
            continue;
        }
        fail("GitHub returned HTTP %ld; retry later", code);
    }
    fail("Too many download redirects");
}
char *http_text(const char *url, size_t limit) { return request(url, NULL, limit, 0); }
void http_download(const char *url, const char *path, uint64_t limit, int progress) {
    free(request(url, path, limit, progress));
}
cJSON *filter_releases(cJSON *releases) {
    if (!cJSON_IsArray(releases) || cJSON_GetArraySize(releases) > 100)
        fail("Invalid release list");
    cJSON *out = cJSON_CreateArray();
    for (cJSON *r = releases->child; r; r = r->next) {
        const char *tag = json_string(r, "tag_name"), *name = json_string(r, "name");
        if (*tag == 'v')
            tag++;
        if (!version_valid(tag) || cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(r, "draft")) ||
            cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(r, "prerelease")) ||
            json_integer(r, "id") <= 0)
            continue;
        const char *os = strstr(name, "(Stock OS ");
        if (!os)
            continue;
        os += 10;
        const char *end = strchr(os, ')');
        if (!end || end - os > 31)
            continue;
        char stock[32];
        memcpy(stock, os, end - os);
        stock[end - os] = 0;
        if (!version_valid(stock))
            continue;
        char wanted[128];
        snprintf(wanted, sizeof(wanted), "Dashborne-%s-microSD.zip", tag);
        cJSON *archive = NULL, *checksum = NULL,
              *assets = cJSON_GetObjectItemCaseSensitive(r, "assets");
        int supported = 0;
        if (!cJSON_IsArray(assets))
            continue;
        for (cJSON *a = assets->child; a; a = a->next) {
            const char *an = json_string(a, "name");
            if (!strcmp(an, wanted)) {
                if (archive)
                    fail("Duplicate release archive");
                archive = a;
            }
            if (!strcmp(an, "SHA256SUMS")) {
                if (checksum)
                    fail("Duplicate checksum asset");
                checksum = a;
            }
            if (!strcmp(an, "updater-v1.json"))
                supported = 1;
        }
        long long size = json_integer(archive, "size");
        if (!archive || !checksum || !allowed_url(json_string(archive, "browser_download_url")) ||
            !allowed_url(json_string(checksum, "browser_download_url")) || size <= 0 ||
            size > 256 * (long long)MIB)
            continue;
        cJSON *c = cJSON_CreateObject();
        cJSON_AddNumberToObject(c, "id", (double)json_integer(r, "id"));
        cJSON_AddStringToObject(c, "version", tag);
        cJSON_AddStringToObject(c, "stock_os", stock);
        char notes[401];
        clean_text(notes, sizeof(notes), json_string(r, "body"));
        cJSON_AddStringToObject(c, "notes", notes);
        cJSON_AddBoolToObject(c, "supported", supported);
        cJSON_AddItemToObject(c, "archive", cJSON_Duplicate(archive, 1));
        cJSON_AddItemToObject(c, "checksum", cJSON_Duplicate(checksum, 1));
        cJSON_AddItemToArray(out, c);
    }
    return out;
}
void list_releases(void) {
    status("Checking GitHub releases...");
    char *body =
        http_text("https://api.github.com/repos/oliexe/Dashborne/releases?per_page=100", 4 * MIB);
    cJSON *rs = parse_json(body), *cs = filter_releases(rs);
    free(body);
    cJSON_Delete(rs);
    write_json(cache_path, cs, 0600);
    char *tsv = allocate(100 * 640 + 1);
    size_t used = 0;
    for (cJSON *c = cs->child; c; c = c->next) {
        int n = snprintf(
            tsv + used, 100 * 640 + 1 - used, "%lld\t%s\t%s\t%s\t%s\n", json_integer(c, "id"),
            json_string(c, "version"), json_string(c, "stock_os"),
            cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(c, "supported")) ? "true" : "false",
            json_string(c, "notes"));
        if (n < 0 || (size_t)n >= 100 * 640 + 1 - used)
            fail("Release list too large");
        used += n;
    }
    atomic_write(list_path, tsv, used, 0600);
    free(tsv);
    cJSON_Delete(cs);
    status("Select a release to view its target OS and notes");
}
void download_release(const char *id) {
    char *end;
    errno = 0;
    long long wanted = strtoll(id, &end, 10);
    if (errno || !*id || *end || wanted <= 0)
        fail("Select a release");
    cJSON *cs = read_json(cache_path, 4 * MIB), *c = NULL;
    for (cJSON *row = cs->child; row; row = row->next)
        if (json_integer(row, "id") == wanted) {
            if (c)
                fail("Duplicate release ID");
            c = row;
        }
    if (!c)
        fail("Release list changed; refresh it");
    if (!cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(c, "supported")))
        fail("This older release requires manual installation");
    const char *stock = installed_os(), *version = json_string(c, "version");
    if (strcmp(stock, json_string(c, "stock_os")))
        fail("Release targets stock OS %s; installed OS is %s", json_string(c, "stock_os"), stock);
    cJSON *archive = cJSON_GetObjectItemCaseSensitive(c, "archive"),
          *checksum = cJSON_GetObjectItemCaseSensitive(c, "checksum");
    long long bytes = json_integer(archive, "size");
    if (bytes <= 0 || bytes > 256 * (long long)MIB)
        fail("Invalid release size; refresh the release list");
    char *sums = http_text(json_string(checksum, "browser_download_url"), 65536),
         expected[65] = {0}, *save = NULL;
    for (char *l = strtok_r(sums, "\n", &save); l; l = strtok_r(NULL, "\n", &save)) {
        char hash[66], name[256], extra;
        if (sscanf(l, "%65s %255s %c", hash, name, &extra) == 2 &&
            !strcmp(name, json_string(archive, "name"))) {
            if (*expected || !hex_digest(hash))
                fail("Invalid or duplicate archive checksum");
            strcpy(expected, hash);
        }
    }
    free(sums);
    if (!*expected)
        fail("Missing archive SHA256");
    const char *digest = json_string(archive, "digest");
    if (*digest && (strncmp(digest, "sha256:", 7) || strcmp(digest + 7, expected)))
        fail("GitHub and release checksums disagree");
    space_check((uint64_t)bytes + 768 * MIB);
    char zip[PATH_CAP], stage[PATH_CAP];
    path_join(zip, work(), "download.zip");
    path_join(stage, work(), "stage");
    status("Downloading release...");
    http_download(json_string(archive, "browser_download_url"), zip, bytes, 1);
    struct stat info;
    char got[65];
    sha_file(zip, got);
    if (stat(zip, &info) || info.st_size != bytes || strcmp(expected, got)) {
        unlink(zip);
        fail("Download checksum failed; installation unchanged");
    }
    status("Verifying and staging release...");
    remove_tree(stage);
    cJSON *m = extract_release(zip, stage, version, stock);
    preserve(stage, m);
    cJSON_Delete(m);
    status("Installing on microSD...");
    apply(stage);
    unlink(zip);
    cJSON_Delete(cs);
}
