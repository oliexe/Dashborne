/* Original C adapter for the inspected ARM FlyThings Activity ABI. */
#include "demo.h"
#include "wifi_debug.h"
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
#define SPEED_ID 50001
#define MODE_ID 50003
#define DRIVE_ID 50004
#define CRUISE_ID 50005
#define MAX_ID 50006
#define PARK_ID 50007
#define HEADER_ID 50010
#define SUMMARY_ID 50011
#define PRIMARY_ID 50012
#define DETAIL_ID 50013
#define PAUSE_ID 50018
#define BACKGROUND_ID 50019
#define ROAD_ID 20001
#define PANE_ID 20002
#define SPEED_BUTTON_ID 20003
#define DEMO_TIMER 91001
#define ASSET "../../../mnt/extsd/ui/cluster/"

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
static int (*control_id)(void *);
static void (*set_text)(void *, const char *);
static void (*set_picture)(void *, const char *);
static void (*set_color)(void *, int);
static void (*register_timer)(void *, int, int);
static void *table[17], *prepared;
static struct demo_state demo;
static int previous_speed = -1, previous_frame = -1, previous_pane = -1, previous_park = -1, previous_pause = -1;
static unsigned int network_ticks;
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

static void label(void *self, int id, const char *text) {
    void *control = find_control(self, id);
    if (control) set_text(control, text);
}
static void picture(void *self, int id, const char *path) {
    void *control = find_control(self, id);
    if (control) set_picture(control, path);
}
static void color(void *self, int id, int rgb) {
    void *control = find_control(self, id);
    if (control) set_color(control, rgb);
}

static void render_demo(void *self) {
    if (demo.speed != previous_speed) {
        char text[4], reverse[3]; unsigned int n = 0, at = 0, value = (unsigned int)demo.speed;
        do { reverse[n++] = (char)('0' + value % 10); value /= 10; } while (value);
        while (n) text[at++] = reverse[--n];
        text[at] = 0;
        label(self, SPEED_ID, text);
        previous_speed = demo.speed;
    }
    int frame = demo.parked ? 8 : (int)demo.frame;
    if (frame != previous_frame) {
        static const char *const roads[] = {
            ASSET "road-0.png", ASSET "road-1.png", ASSET "road-2.png", ASSET "road-3.png",
            ASSET "road-4.png", ASSET "road-5.png", ASSET "road-6.png", ASSET "road-7.png", ASSET "park.png"
        };
        picture(self, ROAD_ID, roads[frame]);
        previous_frame = frame;
    }
    if (demo.energy != previous_pane) {
        picture(self, PANE_ID, demo.energy ? ASSET "energy.png" : ASSET "nav.png");
        label(self, HEADER_ID, demo.energy ? "DEMO · ENERGY" : "DEMO · NAVIGATION");
        label(self, SUMMARY_ID, demo.energy ? "Average consumption · last 30 km" : "16:42 arrival · 9 min · 6.8 km");
        label(self, PRIMARY_ID, demo.energy ? "168 Wh/km" : "600 m");
        label(self, DETAIL_ID, demo.energy ? "Projected range 312 km" : "Keep right · Centrum");
        previous_pane = demo.energy;
    }
    if (demo.parked != previous_park || demo.paused != previous_pause) {
        label(self, MODE_ID, demo.parked ? "Parked" : "Autosteer");
        color(self, MODE_ID, demo.parked ? 0x87929f : 0x559fff);
        color(self, PARK_ID, demo.parked ? 0xf6f7f9 : 0x59616d);
        color(self, DRIVE_ID, demo.parked ? 0x59616d : 0x559fff);
        label(self, CRUISE_ID, demo.parked ? "" : "75");
        label(self, MAX_ID, demo.parked ? "" : "MAX");
        label(self, PAUSE_ID, demo.paused ? "Demo paused" : "");
        if (demo.parked != previous_park)
            picture(self, BACKGROUND_ID, demo.parked ? ASSET "base-park.png" : ASSET "base.png");
        previous_park = demo.parked; previous_pause = demo.paused;
    }
}

static int on_timer(void *self, int id) {
    if (id == DEMO_TIMER) {
        demo_tick(&demo);
        render_demo(self);
        /* The network adapter is polled in seconds; graphics run at 10 Hz. */
        if (++network_ticks == 10) { network_ticks = 0; wifi_debug_poll(); }
    }
    return 1;
}
static int on_timer_secondary(void *self, int id) {
    return on_timer((void **)self - 1, id);
}
static void on_create(void *self) {
    base_on_create(self);
    demo_init(&demo);
    previous_speed = previous_frame = previous_pane = previous_park = previous_pause = -1;
    network_ticks = 0;
    wifi_debug_retry();
    render_demo(self);
    register_timer(self, DEMO_TIMER, 100);
    log_line("cluster-created: simulated-data\n");
}
static void on_click(void *self, void *control) {
    int id = control_id(control);
    if (id == ROAD_ID) { demo_toggle_park(&demo); log_line("demo: park-drive\n"); }
    else if (id == PANE_ID) { demo_toggle_pane(&demo); log_line("demo: navigation-energy\n"); }
    else if (id == SPEED_BUTTON_ID) { demo_toggle_pause(&demo); log_line("demo: pause-resume\n"); }
    render_demo(self);
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
    BIND(set_picture, "_ZN6ZKBase16setBackgroundPicEPKc");
    BIND(set_color, "_ZN10ZKTextView12setTextColorEi");
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
    log_line("dashborne-selected\n");
    return ACTIVITY;
}

EXPORT void onEasyUIDeinit(void *ctx) {
    if (prepared) { destroy(prepared); prepared = (void *)0; }
    log_line("shutdown\n");
    if (stock_deinit) stock_deinit(ctx);
}
