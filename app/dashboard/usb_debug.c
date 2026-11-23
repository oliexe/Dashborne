#include "usb_debug.h"

extern void *dlsym(void *, const char *);
extern int open(const char *, int, ...);
extern int read(int, void *, unsigned int);
extern int write(int, const void *, unsigned int);
extern int close(int);

#define USB_ENABLE "/sys/class/zkswe_usb/zkswe0/enable"
/* Android property_get requires room for PROPERTY_VALUE_MAX (92 bytes). */
#define PROPERTY_BUFFER 128
static int (*get_property)(const char *, char *, const char *);
static int (*set_property)(const char *, const char *);
static void (*log_event)(const char *);
static int owned;
static char previous_tcp_port[PROPERTY_BUFFER];
static const char *error;

static int equal(const char *a, const char *b) {
    while (*a && *a == *b) { ++a; ++b; }
    return *a == *b;
}

static int usb_enabled(void) {
    char value[8];
    int fd = open(USB_ENABLE, 0);
    if (fd < 0) return -1;
    int n = read(fd, value, sizeof(value));
    (void)close(fd);
    if (n < 1 || (value[0] != '0' && value[0] != '1')) return -1;
    return value[0] == '1';
}

static int disable_usb(void) {
    int fd = open(USB_ENABLE, 1);
    if (fd < 0) return -1;
    int n = write(fd, "0\n", 2);
    (void)close(fd);
    return n == 2 ? 0 : -1;
}

static int stop_owned(void) {
    if (!owned) return 0;
    /* Cancel the ADB selection before disabling the gadget. Status polling
       finishes cleanup after init reports the daemon stopped. */
    int failed = set_property("sys.usb.config", "none") != 0;
    failed |= set_property("ctl.stop", "adbd") != 0;
    failed |= disable_usb() != 0;
    failed |= set_property("sys.usb.state", "none") != 0;
    if (failed) { error = "ADB: stop failed; tap to retry"; return -1; }
    owned = 2;
    log_event("adb-stop-requested\n");
    return 0;
}

void usb_debug_bind(void *runtime, void (*logger)(const char *)) {
    log_event = logger;
    /* These exported runtime methods are direct tail calls to property_get
       and property_set in the inspected ARM binary, including their results. */
    *(void **)(&get_property) = dlsym(runtime, "_ZN16SystemProperties9getStringEPKcPcS1_");
    *(void **)(&set_property) = dlsym(runtime, "_ZN16SystemProperties9setStringEPKcS1_");
}

int usb_debug_owned(void) { return owned; }

void usb_debug_toggle(void) {
    error = (const char *)0;
    if (!get_property || !set_property) { error = "ADB: property API unavailable"; return; }
    if (owned) { (void)stop_owned(); return; }
    char daemon[PROPERTY_BUFFER];
    get_property("init.svc.adbd", daemon, "");
    /* A disabled, never-started service may not have published its state yet. */
    if (*daemon && !equal(daemon, "stopped")) {
        error = "ADB: service already active or unknown";
        log_event("adb-start-skipped: service-not-stopped\n");
        return;
    }
    int enabled = usb_enabled();
    if (enabled != 0) {
        error = enabled < 0 ? "ADB: USB interface unavailable" : "ADB: USB already in use";
        log_event("adb-start-skipped: usb-unavailable-or-busy\n");
        return;
    }
    get_property("service.adb.tcp.port", previous_tcp_port, "");
    owned = 1;
    /* Use the existing init service and USB trigger. No persistent properties
       are set. A nonempty zero explicitly selects USB rather than TCP. */
    if (set_property("service.adb.tcp.port", "0") ||
        set_property("sys.usb.config", "none") ||
        set_property("sys.usb.config", "adb") ||
        set_property("ctl.start", "adbd")) {
        error = "ADB: start request failed";
        log_event("adb-start-request-failed\n");
        (void)stop_owned();
        return;
    }
    log_event("adb-start-requested\n");
}

const char *usb_debug_status(void) {
    if (!get_property || !set_property) return "ADB: property API unavailable";
    char daemon[PROPERTY_BUFFER];
    get_property("init.svc.adbd", daemon, "unknown");
    if (owned == 2 && equal(daemon, "stopped")) {
        if (disable_usb() || set_property("sys.usb.state", "none") ||
            set_property("service.adb.tcp.port", previous_tcp_port))
            error = "ADB: stop failed; tap to retry";
        else { owned = 0; log_event("adb-stopped\n"); }
    }
    if (error) return error;
    int enabled = usb_enabled();
    if (enabled < 0) return "ADB: USB interface unavailable";
    if (owned == 2) return "ADB: stopping";
    if (equal(daemon, "running"))
        return enabled ? "ADB: running; USB enabled" : "ADB: running; USB disabled";
    if (owned) return enabled ? "ADB: starting; USB enabled" : "ADB: starting; USB disabled";
    if (equal(daemon, "stopped")) return enabled ? "ADB: stopped; USB enabled" : "ADB: off";
    return "ADB: daemon status unknown";
}

void usb_debug_shutdown(void) {
    if (owned) { (void)stop_owned(); (void)usb_debug_status(); }
}
