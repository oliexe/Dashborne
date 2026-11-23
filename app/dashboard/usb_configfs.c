/* ConfigFS adapter for the controller observed on the development DASH. */
#include "usb_configfs.h"
extern int open(const char *, int, ...);
extern int read(int, void *, unsigned int);
extern int write(int, const void *, unsigned int);
extern int close(int);
extern int access(const char *, int);
extern int mkdir(const char *, unsigned int);
extern int symlink(const char *, const char *);
extern int mount(const char *, const char *, const char *, unsigned long, const void *);
extern int *__errno_location(void);

#define CONTROLLER "4100000.udc-controller"
#define UDC "/sys/class/udc/" CONTROLLER
#define CONFIG "/sys/kernel/config"
#define GADGET CONFIG "/usb_gadget/dashborne"
#define FFS "/dev/usb-ffs/adb"
static int created, prepared, bound, attempts;
static const char *failure;
static void (*log_event)(const char *);

static unsigned int length(const char *s) { unsigned int n = 0; while (s[n]) ++n; return n; }
static int same(const char *a, const char *b) {
    while (*a && *a == *b) { ++a; ++b; }
    return *a == *b;
}

static int fail(const char *message) {
    int code = *__errno_location();
    failure = message;
    log_event(message); log_event("\n");
    char text[32] = "configfs-errno=", digits[10];
    unsigned int value = code > 0 ? (unsigned int)code : 0, n = 0, pos = 14;
    do { digits[n++] = (char)('0' + value % 10); value /= 10; } while (value);
    while (n) text[pos++] = digits[--n];
    text[pos++] = '\n'; text[pos] = 0;
    log_event(text);
    return -1;
}

static int get(const char *path, char *text, unsigned int size) {
    int fd = open(path, 0);
    if (fd < 0) return -1;
    int n = read(fd, text, size - 1);
    (void)close(fd);
    if (n < 0) return -1;
    while (n > 0 && (text[n - 1] == '\n' || text[n - 1] == '\r')) --n;
    text[n] = 0;
    return n;
}

static int put(const char *path, const char *text) {
    int fd = open(path, 1);
    if (fd < 0) return -1;
    unsigned int n = length(text);
    int result = write(fd, text, n);
    int code = *__errno_location();
    (void)close(fd);
    *__errno_location() = code;
    return result == (int)n ? 0 : -1;
}

static int directory(const char *path) { return access(path, 0) == 0 ? 0 : mkdir(path, 0755); }
int usb_configfs_available(void) { return access(UDC, 0) == 0; }
const char *usb_configfs_error(void) { return failure ? failure : "ADB: ConfigFS setup failed"; }

int usb_configfs_prepare(void (*logger)(const char *)) {
    log_event = logger;
    failure = (const char *)0;
    attempts = 0;
    if (prepared) return 0;
    char function[128];
    int n = get(UDC "/function", function, sizeof(function));
    if (n < 0) return fail("ADB: controller state unreadable");
    if (n > 0) return fail("ADB: USB controller already in use");
    if (access(CONFIG "/usb_gadget", 0) &&
        mount("none", CONFIG, "configfs", 0, (void *)0))
        return fail("ADB: ConfigFS mount failed");
    if (!created) {
        if (access(GADGET, 0) == 0) return fail("ADB: reboot to reset debug gadget");
        if (mkdir(GADGET, 0755)) return fail("ADB: gadget creation failed");
        created = 1;
    }
    const char *directories[] = {
        GADGET "/strings/0x409", GADGET "/configs/c.1",
        GADGET "/configs/c.1/strings/0x409", GADGET "/functions/ffs.adb",
        "/dev/usb-ffs", FFS
    };
    for (unsigned int i = 0; i < sizeof(directories) / sizeof(directories[0]); ++i)
        if (directory(directories[i])) return fail("ADB: USB directory setup failed");
    const char *settings[][2] = {
        {GADGET "/idVendor", "0x18d1\n"}, {GADGET "/idProduct", "0xd002\n"},
        {GADGET "/strings/0x409/manufacturer", "Dashborne\n"},
        {GADGET "/strings/0x409/product", "Dashborne ADB\n"},
        {GADGET "/strings/0x409/serialnumber", "dashborne-dev\n"},
        {GADGET "/configs/c.1/MaxPower", "500\n"},
        {GADGET "/configs/c.1/strings/0x409/configuration", "ADB\n"}
    };
    for (unsigned int i = 0; i < sizeof(settings) / sizeof(settings[0]); ++i)
        if (put(settings[i][0], settings[i][1])) return fail("ADB: USB attribute setup failed");
    if (access(GADGET "/configs/c.1/ffs.adb", 0) &&
        symlink(GADGET "/functions/ffs.adb", GADGET "/configs/c.1/ffs.adb"))
        return fail("ADB: USB function link failed");
    if (access(FFS "/ep0", 0) &&
        mount("adb", FFS, "functionfs", 0, "uid=2000,gid=2000"))
        return fail("ADB: FunctionFS mount failed");
    prepared = 1;
    log_event("configfs-prepared\n");
    return 0;
}

int usb_configfs_bind(void) {
    if (bound) return 1;
    if (!prepared) return -1;
    if (access(FFS "/ep1", 0) || access(FFS "/ep2", 0)) {
        if (++attempts >= 15) return fail("ADB: USB endpoints did not appear");
        return 0;
    }
    if (put(GADGET "/UDC", CONTROLLER "\n")) {
        if (++attempts >= 15) return fail("ADB: controller binding failed");
        return 0;
    }
    bound = 1;
    log_event("configfs-controller-bound\n");
    return 1;
}

int usb_configfs_stop(void) {
    if (bound && put(GADGET "/UDC", "\n")) return fail("ADB: USB unbind failed");
    bound = 0;
    return 0;
}

const char *usb_configfs_status(void) {
    static char previous_state[64];
    static int have_state;
    if (failure) return failure;
    if (!bound) return "ADB: waiting for USB endpoints";
    char state[64];
    if (get(UDC "/state", state, sizeof(state)) < 0) return "ADB: bound; USB state unreadable";
    if (!have_state || !same(state, previous_state)) {
        have_state = 1;
        unsigned int i = 0;
        do { previous_state[i] = state[i]; } while (state[i++]);
        log_event("configfs-kernel-state=");
        log_event(*state ? state : "[empty]");
        log_event("\n");
    }
    if (same(state, "configured")) return "ADB: USB configured";
    if (same(state, "not-attached")) return "ADB: waiting for USB host";
    if (same(state, "suspended")) return "ADB: USB suspended";
    if (same(state, "attached")) return "ADB: USB attached; not configured";
    if (same(state, "powered")) return "ADB: USB powered; no address";
    if (same(state, "default")) return "ADB: USB default address";
    if (same(state, "addressed")) return "ADB: USB addressed; not configured";
    if (same(state, "reconnecting")) return "ADB: USB reconnecting";
    if (same(state, "unauthenticated")) return "ADB: USB unauthenticated";
    return "ADB: bound; unknown USB state";
}
