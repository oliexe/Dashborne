#include "tcp_adb.h"
extern void *dlsym(void *, const char *);
extern int open(const char *, int, ...);
extern int read(int, void *, unsigned int);
extern int close(int);
static int (*get_property)(const char *, char *, const char *);
static int (*set_property)(const char *, const char *);
static void (*log_event)(const char *);
static unsigned int cooldown;
static const char *status = "Waiting for Wi-Fi";

static int equal(const char *a, const char *b) {
    while (*a && *a == *b) { ++a; ++b; }
    return *a == *b;
}
static int whitespace(char c) { return c == ' ' || c == '\t'; }
static const char *next_field(const char *s) {
    while (*s && !whitespace(*s)) ++s;
    while (whitespace(*s)) ++s;
    return s;
}
static int listening_line(const char *line) {
    while (whitespace(*line)) ++line;
    line = next_field(line); /* local address:port, following row index */
    while (*line && !whitespace(*line) && *line != ':') ++line;
    if (*line++ != ':') return 0;
    const char *port = "15B3"; /* 5555 in /proc/net/tcp's hexadecimal notation */
    while (*port) if (*line++ != *port++) return 0;
    if (!whitespace(*line)) return 0;
    while (whitespace(*line)) ++line;
    line = next_field(line); /* skip remote address */
    return line[0] == '0' && line[1] == 'A' && whitespace(line[2]);
}
static int listener_in(const char *path) {
    int fd = open(path, 0);
    if (fd < 0) return 0;
    char chunk[512], line[512]; unsigned int at = 0; int n, found = 0, overflow = 0;
    while (!found && (n = read(fd, chunk, sizeof(chunk))) > 0) {
        for (int i = 0; i < n; ++i) {
            if (chunk[i] == '\n') {
                line[at] = 0;
                if (!overflow && listening_line(line)) { found = 1; break; }
                at = 0; overflow = 0;
            } else if (at < sizeof(line) - 1) line[at++] = chunk[i];
            else overflow = 1;
        }
    }
    (void)close(fd);
    return found;
}

void tcp_adb_bind(void *runtime, void (*logger)(const char *)) {
    log_event = logger;
    *(void **)(&get_property) = dlsym(runtime, "_ZN16SystemProperties9getStringEPKcPcS1_");
    *(void **)(&set_property) = dlsym(runtime, "_ZN16SystemProperties9setStringEPKcS1_");
}

void tcp_adb_poll(int network_ready) {
    if (!get_property || !set_property) { status = "ADB control unavailable"; return; }
    if (!network_ready) { status = "Waiting for Wi-Fi"; return; }
    char daemon[128], port[128];
    get_property("init.svc.adbd", daemon, "");
    get_property("service.adb.tcp.port", port, "");
    if (equal(daemon, "running")) {
        if (!equal(port, "5555")) status = "ADB is using another transport";
        else if (listener_in("/proc/net/tcp") || listener_in("/proc/net/tcp6")) status = "Wi-Fi ADB ready";
        else status = "Waiting for ADB listener";
        return;
    }
    if (*daemon && !equal(daemon, "stopped")) { status = "Waiting for ADB service"; return; }
    if (cooldown) { --cooldown; return; }
    cooldown = 10;
    /* Only transient TCP configuration. The daemon is deliberately left running
       when this helper exits, so development UI reloads retain remote access. */
    if (set_property("service.adb.tcp.port", "5555") || set_property("ctl.start", "adbd")) {
        status = "ADB start failed; retrying";
        log_event("adb-start-failed\n");
    } else {
        status = "Starting Wi-Fi ADB";
        log_event("adb-start-requested\n");
    }
}
const char *tcp_adb_status(void) { return status; }
