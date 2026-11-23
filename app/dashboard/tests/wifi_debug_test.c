#include <assert.h>
#include <stdio.h>
#include <string.h>
#define dlsym fake_dlsym
#define open fake_open
#define read fake_read
#define close fake_close
#define socket fake_socket
#define ioctl fake_ioctl
#include "../wifi_debug.c"

static const char *scenario;
static const char *config = "Example WiFi\nexample-passphrase\n";
static unsigned int offset;
static int changes, connects, mode, connected, fail_ip;
static int is(const char *s) { return !strcmp(scenario, s); }
static void log_message(const char *s) { assert(s && !strstr(s, "example-passphrase")); }
static void change(int m) { assert(m == 2); ++changes; }
static int get(void) { return mode; }
static int connection(void) { return connected; }
static void connect_network(const void *s, const void *p) {
    const struct string_view_abi *a = s, *b = p;
    assert(!strcmp(a->data, "Example WiFi") && a->size == 12);
    assert(!strcmp(b->data, "example-passphrase") && b->size == 18);
    ++connects;
}
void *fake_dlsym(void *handle, const char *name) {
    assert(handle == (void *)1);
    if (is("missing-api")) return NULL;
    if (strstr(name, "change_mode")) return (void *)change;
    if (strstr(name, "get_mode")) return (void *)get;
    if (strstr(name, "is_connected")) return (void *)connection;
    assert(strstr(name, "connect")); return (void *)connect_network;
}
int fake_open(const char *path, int flags, ...) {
    assert(!strcmp(path, CONFIG) && flags == 0); offset = 0;
    return is("missing-config") ? -1 : 10;
}
int fake_read(int fd, void *buf, unsigned int count) {
    assert(fd == 10);
    if (is("read-failure")) return -1;
    unsigned int n = (unsigned int)strlen(config) - offset;
    if (n > count) n = count;
    if (n > 3) n = 3; /* Exercise short reads. */
    memcpy(buf, config + offset, n); offset += n; return (int)n;
}
int fake_close(int fd) { assert(fd == 10 || fd == 11); return 0; }
int fake_socket(int domain, int type, int protocol) { assert(domain == 2 && type == 2 && !protocol); return 11; }
int fake_ioctl(int fd, unsigned long request, ...) {
    assert(fd == 11 && request == 0x8915);
    __builtin_va_list ap; __builtin_va_start(ap, request);
    unsigned char *p = __builtin_va_arg(ap, void *); __builtin_va_end(ap);
    assert(!strcmp((char *)p, "wlan0"));
    if (fail_ip) return -1;
    p[16] = 2; p[20] = 192; p[21] = 168; p[22] = 1; p[23] = 234;
    return 0;
}
int main(int argc, char **argv) {
    assert(argc == 2); scenario = argv[1];
    if (is("invalid-config")) config = "Example WiFi\nshort\n";
    wifi_debug_bind((void *)1, log_message);
    wifi_debug_retry();
    if (is("missing-api") || is("missing-config") || is("invalid-config") || is("read-failure")) {
        for (int i = 0; i < 100; ++i) wifi_debug_poll();
        assert(!changes && !connects && !*wifi_debug_ip());
        assert(strstr(wifi_debug_status(), "unavailable") || strstr(wifi_debug_status(), "invalid"));
    } else {
        assert(changes == 1 && connects == 0);
        for (int i = 0; i < 8; ++i) wifi_debug_poll();
        assert(!connects); /* Wait for stock mode transition. */
        mode = 2;
        wifi_debug_poll(); assert(connects == 1);
        if (is("timeout")) {
            for (int i = 0; i < 100; ++i) wifi_debug_poll();
            assert(connects == 1 && strstr(wifi_debug_status(), "retry"));
            wifi_debug_retry();
            assert(changes == 2);
            for (int i = 0; i < 5; ++i) wifi_debug_poll();
            assert(connects == 2);
        }
        connected = 1; fail_ip = 1;
        wifi_debug_poll(); assert(!*wifi_debug_ip());
        fail_ip = 0; wifi_debug_poll();
        assert(!strcmp(wifi_debug_ip(), "192.168.1.234"));
        assert(!strcmp(wifi_debug_status(), "Wi-Fi: 192.168.1.234"));
        connected = 0; wifi_debug_poll(); assert(!*wifi_debug_ip());
    }
    printf("PASS: Wi-Fi %s\n", scenario);
}
