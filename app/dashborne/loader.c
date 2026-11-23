/* Original C adapter for the inspected ARM FlyThings Activity ABI. */
#include "../adb-enabler/tcp_adb.h"
#include "state.h"
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
#define ACTIVITY "dashborneLoaderActivity"
#define LAYOUT "/mnt/extsd/Dashborne/ui/loader.ftu"
#define BOOT_TIMER 92001
#define NETWORK_TIMER 92002
#define SPLASH_ID 110001
#define CONFIG_ID 110002
#define TAP_ID 20001
#define WIFI_ID 20002
#define LAUNCH_ID 20003
#define PREVIOUS_ID 20004
#define NEXT_ID 20005
#define ROW_ID 20100
#define DETAIL_ID 50100
#define STATUS_ID 50002
#define ADDRESS_ID 50003
#define NOTICE_ID 50004
#define PAGE_ID 50005

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
static unsigned int reconnect_ticks, official_ticks;
static struct settings settings;
static int phase, page, settings_ok = 1;
static int (*control_id)(void *);
static void (*set_visible)(void *, int), (*set_color)(void *, int);
static void (*unregister_timer)(void *, int);
static void *(*ui_instance)(void);
static void (*open_activity)(void *, const char *, void *);
static int ready;

static void log_line(const char *s) {
    unsigned int n = 0;
    while (s[n]) ++n;
    int fd = open("/mnt/extsd/Dashborne/loader.log", 1 | 0100 | 02000, 0644);
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
    return "../../../mnt/extsd/Dashborne/ui/loader.ftu";
}

static void label(void *self, int id, const char *text) {
    void *p = find_control(self, id);
    if (p) set_text(p, text);
}
static void visible(void *self, int id, int show) {
    void *p = find_control(self, id);
    if (p) set_visible(p, show);
}
static void color(void *self, int id, int rgb) {
    void *p = find_control(self, id);
    if (p) set_color(p, rgb);
}
static unsigned int append(char *to, unsigned int at, const char *s, unsigned int cap) {
    while (*s && at + 1 < cap) to[at++] = *s++;
    to[at] = 0; return at;
}
static void render(void *self) {
    visible(self, SPLASH_ID, phase == 0);
    visible(self, CONFIG_ID, phase == 1 || phase == 2);
    if (phase != 1 && phase != 2) return;
    label(self, WIFI_ID, settings.wifi_adb ? "[x]  Wi-Fi ADB" : "[ ]  Wi-Fi ADB");
    color(self, WIFI_ID, settings.wifi_adb ? 0x8eb5ff : 0xd8deeb);
    const char *ip = wifi_debug_ip();
    label(self, STATUS_ID, settings.wifi_adb ? (*ip ? tcp_adb_status() : wifi_debug_status()) : "Disabled");
    char address[40] = "";
    if (settings.wifi_adb && *ip) {
        unsigned int n = append(address, 0, ip, sizeof(address));
        append(address, n, ":5555", sizeof(address));
    }
    label(self, ADDRESS_ID, address);
    for (int row = 0; row < 4; ++row) {
        unsigned int index = (unsigned int)(page * 4 + row);
        int show = index < settings.count;
        visible(self, ROW_ID + row, show); visible(self, DETAIL_ID + row, show);
        if (show) {
            struct patch_entry *p = &settings.patches[index];
            char title[100];
            unsigned int n = append(title, 0, p->enabled ? "[x]  " : "[ ]  ", sizeof(title));
            append(title, n, p->title, sizeof(title));
            label(self, ROW_ID + row, title);
            label(self, DETAIL_ID + row, p->compatible ? p->description : "Not compatible with this installed app");
            color(self, ROW_ID + row, !p->compatible ? 0x657080 : p->enabled ? 0x8eb5ff : 0xd8deeb);
        }
    }
    visible(self, PREVIOUS_ID, page > 0);
    visible(self, NEXT_ID, (unsigned int)((page + 1) * 4) < settings.count);
    label(self, PAGE_ID, settings.count ? "Patches on microSD" : "No patches found on microSD");
    label(self, NOTICE_ID, settings_ok ? "Settings are saved to microSD" : "Could not save settings to microSD");
}
static void launch(void *self) {
    if (phase == 2 || phase == 3) return;
    if (!state_save(&settings)) { settings_ok = 0; phase = 1; render(self); return; }
    phase = 2; unregister_timer(self, BOOT_TIMER);
    label(self, NOTICE_ID, "Applying selected patches...");
    int result = state_apply(&settings, log_line);
    if (result < 0) {
        phase = 1; render(self);
        label(self, NOTICE_ID, "Patch could not be applied. Uncheck it to continue.");
        return;
    }
    phase = 3; official_ticks = 0; log_line("official-selected\n");
    open_activity(ui_instance(), stock_screen, (void *)0);
}
static int on_timer(void *self, int id) {
    if (id == BOOT_TIMER && phase == 0) { launch(self); return 1; }
    if (id == NETWORK_TIMER && settings.wifi_adb) {
        if (phase == 3) {
            if (++official_ticks == 15) { wifi_debug_retry(); reconnect_ticks = 0; }
            if (official_ticks < 15) return 1;
        }
        wifi_debug_poll();
        if (*wifi_debug_ip()) reconnect_ticks = 0;
        else if (++reconnect_ticks >= 95) { reconnect_ticks = 0; wifi_debug_retry(); }
        tcp_adb_poll(*wifi_debug_ip() != 0);
        if (phase == 1) render(self);
    }
    return 1;
}
static int on_timer_secondary(void *self, int id) {
    return on_timer((void **)self - 1, id);
}
static void on_create(void *self) {
    base_on_create(self);
    state_load(&settings);
    phase = 0; page = 0;
    /* A one-shot deployment marker opens configuration for remote verification. */
    if (access("/mnt/extsd/Dashborne/config.once", 0) == 0) {
        extern int unlink(const char *);
        (void)unlink("/mnt/extsd/Dashborne/config.once"); phase = 1;
    }
    if (settings.wifi_adb) wifi_debug_retry();
    else tcp_adb_stop();
    render(self);
    register_timer(self, NETWORK_TIMER, 1000);
    if (!phase) register_timer(self, BOOT_TIMER, 3000);
    log_line(phase ? "configuration-created\n" : "splash-created\n");
}
static void on_click(void *self, void *control) {
    int id = control_id(control);
    if (id == TAP_ID && phase == 0) {
        phase = 1; unregister_timer(self, BOOT_TIMER); log_line("configuration-entered\n");
    } else if (phase != 1) return;
    else if (id == LAUNCH_ID) { launch(self); return; }
    else if (id == WIFI_ID) {
        settings.wifi_adb = !settings.wifi_adb;
        settings_ok = state_save(&settings);
        if (settings.wifi_adb) { reconnect_ticks = 0; wifi_debug_retry(); }
        else tcp_adb_stop();
    } else if (id >= ROW_ID && id < ROW_ID + 4) {
        unsigned int index = (unsigned int)(page * 4 + id - ROW_ID);
        if (index < settings.count && settings.patches[index].compatible) {
            settings.patches[index].enabled = !settings.patches[index].enabled;
            settings_ok = state_save(&settings);
        }
    } else if (id == PREVIOUS_ID && page > 0) --page;
    else if (id == NEXT_ID && (unsigned int)((page + 1) * 4) < settings.count) ++page;
    render(self);
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
    BIND(unregister_timer, "_ZN7BaseApp15unregisterTimerEi");
    BIND(control_id, "_ZNK6ZKBase5getIDEv");
    BIND(set_visible, "_ZN6ZKBase10setVisibleEb");
    BIND(set_color, "_ZN10ZKTextView12setTextColorEi");
    BIND(ui_instance, "_ZN13EasyUIContext11getInstanceEv");
    BIND(open_activity, "_ZN13EasyUIContext12openActivityEPKcP6Intent");
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
    log_line("dashborne-selected\n");
    return ACTIVITY;
}

EXPORT void onEasyUIDeinit(void *ctx) {
    /* Keep the init-owned ADB daemon available across UI replacements. */
    if (prepared) { destroy(prepared); prepared = (void *)0; }
    log_line("shutdown\n");
    if (stock_deinit) stock_deinit(ctx);
}
