#include <assert.h>
#include <stdio.h>
#include <string.h>
#define dlsym fake_dlsym
#define open fake_open
#define read fake_read
#define close fake_close
#include "../tcp_adb.c"
static const char *scenario, *daemon = "stopped";
static char port[128] = "";
static int writes, starts, offset;
static int listening, failure;
static int is(const char *s) { return !strcmp(scenario, s); }
static int get(const char *key, char *out, const char *fallback) {
    (void)fallback;
    const char *value;
    if (!strcmp(key, "init.svc.adbd")) value = daemon;
    else { assert(!strcmp(key, "service.adb.tcp.port")); value = port; }
    return snprintf(out, 128, "%s", value);
}
static int set(const char *key, const char *value) {
    ++writes;
    if (failure) return -1;
    if (!strcmp(key, "service.adb.tcp.port")) { assert(!strcmp(value, "5555")); strcpy(port, value); }
    else { assert(!strcmp(key, "ctl.start") && !strcmp(value, "adbd")); ++starts; }
    return 0;
}
void *fake_dlsym(void *handle, const char *name) {
    assert(handle == (void *)2);
    if (is("missing-api")) return NULL;
    return strstr(name, "getString") ? (void *)get : (void *)set;
}
static void logger(const char *s) { assert(s && *s); }
int fake_open(const char *path, int flags, ...) {
    assert(!flags && (!strcmp(path, "/proc/net/tcp") || !strcmp(path, "/proc/net/tcp6")));
    offset = 0; return 10;
}
int fake_read(int fd, void *buffer, unsigned int count) {
    assert(fd == 10);
    const char *data = listening ? "  sl local_address rem_address st\n 0: 00000000:15B3 00000000:0000 0A 0:0\n" :
                                   " 0: 00000000:15B3 00000000:0000 01 0:0\n";
    unsigned int n = (unsigned int)strlen(data) - (unsigned int)offset;
    if (n > count) n = count;
    if (n > 7) n = 7;
    memcpy(buffer, data + offset, n); offset += (int)n; return (int)n;
}
int fake_close(int fd) { assert(fd == 10); return 0; }
int main(int argc, char **argv) {
    assert(argc == 2); scenario = argv[1];
    tcp_adb_bind((void *)2, logger);
    if (is("missing-api")) {
        tcp_adb_poll(1); assert(!writes && strstr(tcp_adb_status(), "unavailable"));
    } else if (is("existing") || is("other-transport")) {
        daemon = "running"; strcpy(port, is("existing") ? "5555" : "0"); listening = 1;
        tcp_adb_poll(1); assert(!writes && strstr(tcp_adb_status(), is("existing") ? "ready" : "another transport"));
    } else {
        tcp_adb_poll(0); assert(!writes);
        if (is("failure")) failure = 1;
        tcp_adb_poll(1);
        if (failure) {
            assert(!starts && strstr(tcp_adb_status(), "failed"));
            int before = writes;
            tcp_adb_poll(1); assert(writes == before);
            failure = 0;
            for (int i = 0; i < 11; ++i) tcp_adb_poll(1);
        }
        assert(starts == 1 && !strcmp(port, "5555"));
        daemon = "running";
        tcp_adb_poll(1); assert(!strcmp(tcp_adb_status(), "Waiting for ADB listener"));
        listening = 1; tcp_adb_poll(1); assert(!strcmp(tcp_adb_status(), "Wi-Fi ADB ready"));
        tcp_adb_poll(0); assert(!strcmp(tcp_adb_status(), "Waiting for Wi-Fi"));
    }
    assert(!listening_line(""));
    assert(!listening_line(" 0: x:15B3 x:0 01 "));
    assert(!listening_line(" 0: x:115B3 x:0 0A "));
    assert(!listening_line(" 0: x:15B3"));
    assert(listening_line(" 0: 00000000:15B3 00000000:0000 0A "));
    printf("PASS: enabler TCP %s\n", scenario);
}
