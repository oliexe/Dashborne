/* Original C adapter for the inspected ARM FlyThings Activity ABI. */
#include "usb_debug.h"
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
#define ACTIVITY "dashborneActivity"
#define LAYOUT "/mnt/extsd/ui/dashborne.ftu"
#define COUNT_ID 50003
#define TAP_ID 20001
#define STOCK_ID 20002
#define DEBUG_ID 20003
#define DEBUG_STATUS_ID 50004
#define DEBUG_TIMER 91001

static void *stock, *runtime, *context;
static void (*stock_init)(void *), (*stock_deinit)(void *);
static const char *(*stock_start)(void *);
static const char *stock_screen;
static void *(*factory_instance)(void);
static void (*register_activity)(void *, const char *, void *(*)(void));
static void (*activity_ctor)(void *), (*activity_dtor)(void *);
static int (*base_create)(void *);
static void (*base_on_create)(void *);
static void *(*find_control)(void *, int);
static int (*control_id)(void *);
static void (*set_text)(void *, const char *);
static void (*open_activity)(void *, const char *, void *);
static void (*register_timer)(void *, int, int);
static void *table[17], *prepared;
static unsigned int taps;
static int ready;

static void log_line(const char *s) {
    unsigned int n = 0;
    while (s[n]) ++n;
    int fd = open("/mnt/extsd/Dashborne/events.log", 1 | 0100 | 02000, 0644);
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
    return "../../../mnt/extsd/ui/dashborne.ftu";
}

static void show_count(void *self) {
    char text[32] = "Touches: ", digits[10];
    unsigned int value = taps, n = 0, at = 9;
    do { digits[n++] = (char)('0' + value % 10); value /= 10; } while (value);
    while (n) text[at++] = digits[--n];
    text[at] = 0;
    void *label = find_control(self, COUNT_ID);
    if (label) set_text(label, text);
}

static void show_debug(void *self) {
    usb_debug_poll();
    static const char *previous_status;
    const char *status = usb_debug_status();
    void *label = find_control(self, DEBUG_STATUS_ID);
    if (label) set_text(label, status);
    void *button = find_control(self, DEBUG_ID);
    if (button) set_text(button, usb_debug_owned() ? "Disable USB ADB" : "Enable USB ADB");
    if (status != previous_status) {
        log_line(status);
        log_line("\n");
        previous_status = status;
    }
}

static int on_timer(void *self, int id) {
    if (id == DEBUG_TIMER) show_debug(self);
    return 1;
}

static int on_timer_secondary(void *self, int id) {
    return on_timer((void **)self - 1, id);
}

static void on_create(void *self) {
    base_on_create(self); /* Wire the framework's button listeners. */
    usb_debug_collect();
    taps = 0;
    show_count(self);
    show_debug(self);
    register_timer(self, DEBUG_TIMER, 1000);
    log_line("screen-created\n");
}

static void on_click(void *self, void *control) {
    int id = control_id(control);
    if (id == TAP_ID) {
        ++taps;
        show_count(self);
        log_line("touch\n");
    } else if (id == DEBUG_ID) {
        usb_debug_toggle();
        show_debug(self);
    } else if (id == STOCK_ID && stock_screen && *stock_screen) {
        log_line("open-original\n");
        open_activity(context, stock_screen, (void *)0);
    }
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
    BIND(control_id, "_ZNK6ZKBase5getIDEv");
    BIND(set_text, "_ZN10ZKTextView7setTextEPKc");
    BIND(open_activity, "_ZN13EasyUIContext12openActivityEPKcP6Intent");
    BIND(register_timer, "_ZN7BaseApp13registerTimerEii");
#undef BIND
    void **original = dlsym(runtime, "_ZTV8Activity");
    if (!original || original[6] != (void *)base_on_create ||
        original[5] != dlsym(runtime, "_ZNK7BaseApp10getAppNameEv") ||
        (__PTRDIFF_TYPE__)original[12] != -(__PTRDIFF_TYPE__)sizeof(void *)) return 0;
    for (unsigned int i = 0; i < 17; ++i) table[i] = original[i];
    table[3] = (void *)destroy;
    table[4] = (void *)on_click;
    table[5] = (void *)layout_name;
    table[6] = (void *)on_create;
    table[7] = (void *)on_timer;
    table[15] = (void *)destroy_secondary;
    table[16] = (void *)on_timer_secondary;
    usb_debug_bind(runtime, log_line);
    return 1;
}

EXPORT void onEasyUIInit(void *ctx) {
    context = ctx;
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
    log_line("dashborne-selected\n");
    return ACTIVITY;
}

EXPORT void onEasyUIDeinit(void *ctx) {
    usb_debug_shutdown();
    if (prepared) { destroy(prepared); prepared = (void *)0; }
    log_line("shutdown\n");
    if (stock_deinit) stock_deinit(ctx);
}
