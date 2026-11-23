/* Original C adapter for the inspected ARM FlyThings Activity ABI. */
#include "tcp_adb.h"
#include "../dashboard/wifi_debug.h"
typedef __SIZE_TYPE__ usize;
typedef unsigned int u32;
extern void *dlopen(const char *, int);
extern void *dlsym(void *, const char *);
extern void *calloc(usize, usize);
extern void free(void *);
extern int open(const char *, int, ...);
extern int read(int, void *, unsigned int);
extern int write(int, const void *, unsigned int);
extern int close(int);
extern int access(const char *, int);
extern int fsync(int);

#ifndef RUNTIME_SIZE
#define RUNTIME_SIZE 826052u
#define RUNTIME_HASH 0x3ca7071fu
#endif
#define EXPORT __attribute__((visibility("default")))
#define ACTIVITY "adbEnablerActivity"
#define LAYOUT "/mnt/extsd/ui/adb-enabler.ftu"
#define STATUS_ID 50002
#define ADDRESS_ID 50003
#define DEBUG_TIMER 91002

static void *stock, *runtime;
static void (*stock_init)(void *), (*stock_deinit)(void *);
static const char *(*stock_start)(void *);
static const char *stock_screen;
static void *(*factory_instance)(void);
static void (*register_activity)(void *, const char *, void *(*)(void));
static void (*activity_ctor)(void *), (*activity_dtor)(void *);
static int (*base_create)(void *);
static void (*base_on_create)(void *);
static void *(*find_control)(void *, int);
static void (*set_text)(void *, const char *);
static void (*register_timer)(void *, int, int);
static void *table[17], *prepared;
static unsigned int reconnect_ticks;
static int ready;

static void log_line(const char *s) {
    unsigned int n = 0;
    while (s[n]) ++n;
    int fd = open("/mnt/extsd/adb-enabler/events.log", 1 | 0100 | 02000, 0644);
    if (fd >= 0) { (void)write(fd, s, n); (void)fsync(fd); (void)close(fd); }
}

/* Compatibility fingerprint, not an authenticity or security check. */
static int compatible_runtime(void) {
    unsigned char buf[1024];
    u32 hash = 2166136261u, total = 0;
    int fd = open("/lib/libeasyui.so", 0), n;
    if (fd < 0) return 0;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        total += (u32)n;
        for (int i = 0; i < n; ++i) hash = (hash ^ buf[i]) * 16777619u;
    }
    (void)close(fd);
    return n == 0 && total == RUNTIME_SIZE && hash == RUNTIME_HASH;
}

static const char *layout_name(void *self) {
    (void)self;
    /* The runtime concatenates its resource root rather than accepting an
       absolute path. Both /res/ui/ and /mnt/extsd/ui/ resolve this to LAYOUT. */
    return "../../../mnt/extsd/ui/adb-enabler.ftu";
}

static void render(void *self) {
    const char *ip = wifi_debug_ip();
    const char *status = *ip ? tcp_adb_status() : wifi_debug_status();
    char address[32] = "Waiting for Wi-Fi";
    if (*ip) {
        unsigned int n = 0;
        while (ip[n] && n < 15) { address[n] = ip[n]; ++n; }
        const char *suffix = ":5555";
        while ((address[n++] = *suffix++)) {}
    }
    void *label = find_control(self, STATUS_ID);
    if (label) set_text(label, status);
    label = find_control(self, ADDRESS_ID);
    if (label) set_text(label, address);
    static char previous[128];
    unsigned int i = 0;
    while (i < sizeof(previous) - 1 && status[i] && status[i] == previous[i]) ++i;
    if (status[i] != previous[i]) {
        log_line(status); log_line("\n");
        for (i = 0; i < sizeof(previous) - 1 && status[i]; ++i) previous[i] = status[i];
        previous[i] = 0;
    }
}

static int on_timer(void *self, int id) {
    if (id == DEBUG_TIMER) {
        wifi_debug_poll();
        if (*wifi_debug_ip()) reconnect_ticks = 0;
        else if (++reconnect_ticks >= 95) { reconnect_ticks = 0; wifi_debug_retry(); }
        tcp_adb_poll(*wifi_debug_ip() != 0);
        render(self);
    }
    return 1;
}

static int on_timer_secondary(void *self, int id) {
    return on_timer((void **)self - 1, id);
}

static void on_create(void *self) {
    base_on_create(self);
    wifi_debug_retry();
    render(self);
    register_timer(self, DEBUG_TIMER, 1000);
    log_line("screen-created\n");
}

static void destroy(void *self) { activity_dtor(self); free(self); }
static void destroy_secondary(void *self) { destroy((void **)self - 1); }

static void *new_activity(void) {
    if (prepared) { void *p = prepared; prepared = (void *)0; return p; }
    /* The inspected Activity occupies 24 bytes on ARM (six pointer words). */
    void **p = calloc(6, sizeof(void *));
    if (!p) return (void *)0;
    activity_ctor(p);
    p[0] = table + 2;
    p[1] = table + 14;
    return p;
}

static int bind_runtime(void) {
    runtime = dlopen("/lib/libeasyui.so", 1);
    if (!runtime) return 0;
#define BIND(var, symbol) do { *(void **)(&(var)) = dlsym(runtime, symbol); if (!(var)) return 0; } while (0)
    BIND(factory_instance, "_ZN15ActivityFactory11getInstanceEv");
    BIND(register_activity, "_ZN15ActivityFactory16registerActivityEPKcPFP8ActivityvE");
    BIND(activity_ctor, "_ZN8ActivityC1Ev");
    BIND(activity_dtor, "_ZN8ActivityD1Ev");
    BIND(base_create, "_ZN7BaseApp6createEv");
    BIND(base_on_create, "_ZN7BaseApp8onCreateEv");
    BIND(find_control, "_ZN7BaseApp15findControlByIDEi");
    BIND(set_text, "_ZN10ZKTextView7setTextEPKc");
    BIND(register_timer, "_ZN7BaseApp13registerTimerEii");
#undef BIND
    void **original = dlsym(runtime, "_ZTV8Activity");
    if (!original || original[6] != (void *)base_on_create ||
        original[5] != dlsym(runtime, "_ZNK7BaseApp10getAppNameEv") ||
        (__PTRDIFF_TYPE__)original[12] != -(__PTRDIFF_TYPE__)sizeof(void *)) return 0;
    for (unsigned int i = 0; i < 17; ++i) table[i] = original[i];
    table[3] = (void *)destroy;
    table[5] = (void *)layout_name;
    table[6] = (void *)on_create;
    table[7] = (void *)on_timer;
    table[15] = (void *)destroy_secondary;
    table[16] = (void *)on_timer_secondary;
    tcp_adb_bind(runtime, log_line);
    wifi_debug_bind(stock, log_line);
    return 1;
}

EXPORT void onEasyUIInit(void *ctx) {
    log_line("startup\n");
    stock = dlopen("/res/lib/libzkgui.so", 1);
    if (!stock) { log_line("original-library-unavailable\n"); return; }
    *(void **)(&stock_init) = dlsym(stock, "onEasyUIInit");
    *(void **)(&stock_deinit) = dlsym(stock, "onEasyUIDeinit");
    *(void **)(&stock_start) = dlsym(stock, "onStartupApp");
    if (stock_init) stock_init(ctx);
    if (!stock_init || !stock_deinit || !stock_start ||
        !compatible_runtime() || access(LAYOUT, 4) || !bind_runtime()) {
        log_line("original-selected: compatibility-or-layout\n");
        return;
    }
    register_activity(factory_instance(), ACTIVITY, new_activity);
    ready = 1;
}

EXPORT const char *onStartupApp(void *ctx) {
    stock_screen = stock_start ? stock_start(ctx) : (const char *)0;
    if (!ready || !stock_screen || !*stock_screen) return stock_screen;
    /* Pre-create the screen so layout parsing failure can fall back to stock. */
    void *p = new_activity();
    if (!p || !base_create(p)) {
        if (p) destroy(p);
        log_line("original-selected: screen-create-failed\n");
        return stock_screen;
    }
    prepared = p;
    log_line("adb-enabler-selected\n");
    return ACTIVITY;
}

EXPORT void onEasyUIDeinit(void *ctx) {
    /* Keep the init-owned ADB daemon available across UI replacements. */
    if (prepared) { destroy(prepared); prepared = (void *)0; }
    log_line("shutdown\n");
    if (stock_deinit) stock_deinit(ctx);
}
