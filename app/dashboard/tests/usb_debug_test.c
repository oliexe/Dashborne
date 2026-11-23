#include <assert.h>
#include <stdio.h>
#include <string.h>
#define dlsym fake_dlsym
#define open fake_open
#define read fake_read
#define write fake_write
#define close fake_close
#define fork fake_fork
#define waitpid fake_waitpid
#define fsync fake_fsync
#include "../usb_debug.c"

static const char *scenario, *daemon = "stopped";
static int gadget, writes, starts, stops, failure_used;
static int forks, polls, flushes;
static char last_log[128];
static char tcp_port[PROPERTY_BUFFER] = "5555";
static char usb_config[PROPERTY_BUFFER] = "none";
static int is(const char *s) { return !strcmp(scenario, s); }
static void log_message(const char *s) { assert(s && *s); snprintf(last_log, sizeof(last_log), "%s", s); }
int fake_fork(void) { ++forks; return 123; }
int fake_waitpid(int pid, int *status, int flags) {
    assert(pid == 123 && flags == 1);
    if (!polls++) return 0;
    *status = is("diagnostic-failure") ? 127 << 8 : 0;
    return pid;
}
int fake_fsync(int fd) { assert(fd == 20); ++flushes; return 0; }
static int get(const char *name, char *out, const char *fallback) {
    const char *value = fallback;
    if (!strcmp(name, "init.svc.adbd")) value = daemon;
    else if (!strcmp(name, "service.adb.tcp.port")) value = tcp_port;
    else assert(0);
    return snprintf(out, PROPERTY_BUFFER, "%s", value);
}
static int set(const char *name, const char *value) {
    assert(strncmp(name, "persist.", 8));
    ++writes;
    if ((!strcmp(name, "sys.usb.config") && !strcmp(value, "adb") && is("start-failure")) ||
        (!strcmp(name, "ctl.stop") && is("stop-failure") && !failure_used++)) return -1;
    if (!strcmp(name, "service.adb.tcp.port")) snprintf(tcp_port, sizeof(tcp_port), "%s", value);
    else if (!strcmp(name, "sys.usb.config")) snprintf(usb_config, sizeof(usb_config), "%s", value);
    else if (!strcmp(name, "ctl.start")) {
        assert(!strcmp(value, "adbd") && !strcmp(tcp_port, "0") && !strcmp(usb_config, "adb"));
        ++starts; /* Simulate init processing the request asynchronously. */
    } else if (!strcmp(name, "ctl.stop")) { assert(!strcmp(value, "adbd")); ++stops; }
    else { assert(!strcmp(name, "sys.usb.state") && !strcmp(value, "none")); }
    return 0;
}
void *fake_dlsym(void *handle, const char *name) {
    assert(handle == (void *)2);
    if (is("missing-api")) return NULL;
    if (!strcmp(name, "_ZN16SystemProperties9getStringEPKcPcS1_")) return (void *)get;
    assert(!strcmp(name, "_ZN16SystemProperties9setStringEPKcS1_")); return (void *)set;
}
int fake_open(const char *path, int flags, ...) {
    if (!strcmp(path, REPORT)) { assert(flags & 1); return 20; }
    assert(!strcmp(path, USB_ENABLE) && (flags == 0 || flags == 1));
    if (is("missing-usb")) return -1;
    if (flags == 1 && is("write-denied")) return -1;
    return flags ? 11 : 10;
}
int fake_read(int fd, void *buf, unsigned int count) {
    assert(fd == 10 && count >= 2);
    memcpy(buf, gadget ? "1\n" : "0\n", 2); return 2;
}
int fake_write(int fd, const void *buf, unsigned int count) {
    assert(fd == 11 && count == 2 && !memcmp(buf, "0\n", 2)); gadget = 0; return 2;
}
int fake_close(int fd) { assert(fd == 10 || fd == 11 || fd == 20); return 0; }

int main(int argc, char **argv) {
    assert(argc == 2); scenario = argv[1];
    usb_debug_bind((void *)2, log_message);
    if (is("diagnostics") || is("diagnostic-failure")) {
        usb_debug_collect(); usb_debug_collect();
        assert(forks == 1);
        usb_debug_poll(); assert(diagnostic_pid == 123 && !flushes);
        usb_debug_poll(); assert(!diagnostic_pid && flushes == 1);
        assert(strstr(last_log, is("diagnostic-failure") ? "failed" : "saved"));
        assert(!writes && !starts && !stops);
        printf("PASS: %s\n", scenario); return 0;
    }
    if (is("external-adb")) daemon = "running";
    if (is("unknown-service")) daemon = "unknown";
    if (is("unset-service")) daemon = "";
    if (is("busy-usb")) gadget = 1;
    usb_debug_toggle();
    if (is("missing-api") || is("missing-usb") || is("external-adb") || is("unknown-service") || is("busy-usb")) {
        assert(!usb_debug_owned() && !writes && !starts);
        assert(strcmp(usb_debug_status(), "ADB: off"));
        usb_debug_shutdown(); assert(!writes);
    } else if (is("start-failure")) {
        assert(!starts && stops == 1);
        assert(!strcmp(usb_debug_status(), "ADB: start request failed"));
        assert(!usb_debug_owned() && !gadget && !strcmp(tcp_port, "5555"));
    } else {
        assert(starts == 1 && usb_debug_owned() && !strcmp(tcp_port, "0"));
        assert(!strcmp(usb_debug_status(), "ADB: starting; USB disabled"));
        daemon = "running"; gadget = 1;
        assert(!strcmp(usb_debug_status(), "ADB: running; USB enabled"));
        if (is("shutdown")) usb_debug_shutdown(); else usb_debug_toggle();
        if (is("stop-failure") || is("write-denied")) {
            assert(usb_debug_owned());
            assert(!strcmp(usb_debug_status(), "ADB: stop failed; tap to retry"));
            if (is("write-denied")) { puts("PASS: write-denied"); return 0; }
            usb_debug_toggle();
        }
        assert(usb_debug_owned());
        assert(!strcmp(usb_debug_status(), "ADB: stopping"));
        assert(!strcmp(tcp_port, "0"));
        /* A queued init action briefly re-enables USB after the stop request. */
        gadget = 1; daemon = "stopped";
        assert(!strcmp(usb_debug_status(), "ADB: off"));
        assert(!gadget && !usb_debug_owned() && !strcmp(tcp_port, "5555"));
        assert(!strcmp(usb_config, "none"));
    }
    printf("PASS: USB ADB %s\n", scenario);
}
