/* Original C adapter for the inspected ARM FlyThings Activity ABI. */
#include "debug.h"
#include "../extensions/engine.h"
#include "state.h"
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
#define PATCHES_TAB_ID 20006
#define ADB_TAB_ID 20007
#define ADB_PANEL_ID 110003
#define PATCHES_PANEL_ID 110004
#define LIST_ID 80001
#define DETAIL_ID 24001
#define TOGGLE_ID 24002
#define WIFI_TOGGLE_ID 50016
#define COUNT_ID 50018
#define EMPTY_ID 50019
#define STATUS_ID 50002
#define ADDRESS_ID 50003
#define NOTICE_ID 50004

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
static struct settings settings;
static int phase, section, settings_ok = 1;
static const char *launch_error;
static void *list_owner;
static void (*set_adapter)(void *, void *), (*set_list_listener)(void *, void *);
static void (*refresh_list)(void *);
static void *(*find_subitem)(void *, int);
static int (*control_id)(void *);
static void (*set_visible)(void *, int), (*set_color)(void *, int), (*set_selected)(void *, int);
static void (*unregister_timer)(void *, int);
static void *(*ui_instance)(void);
static void (*open_activity)(void *, const char *, void *);
static int ready, extension_recovery_ok = 1;

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
static unsigned int number(char *to, unsigned int at, unsigned int value) {
    if (value >= 10) to[at++] = (char)('0' + value / 10);
    to[at++] = (char)('0' + value % 10); to[at] = 0;
    return at;
}
static void render(void *self) {
    visible(self, SPLASH_ID, phase == 0);
    visible(self, CONFIG_ID, phase == 1 || phase == 2);
    if (phase != 1 && phase != 2) return;
    visible(self, PATCHES_PANEL_ID, section == 0);
    visible(self, ADB_PANEL_ID, section == 1);
    visible(self, 50020, section == 0); visible(self, 50021, section == 1);
    void *patch_tab = find_control(self, PATCHES_TAB_ID), *adb_tab = find_control(self, ADB_TAB_ID);
    if (patch_tab) set_selected(patch_tab, section == 0);
    if (adb_tab) set_selected(adb_tab, section == 1);
    color(self, PATCHES_TAB_ID, section == 0 ? 0xf0f2ff : 0x79849a);
    color(self, ADB_TAB_ID, section == 1 ? 0xf0f2ff : 0x79849a);
    label(self, WIFI_ID, "Enable ADB");
    label(self, WIFI_TOGGLE_ID, settings.wifi_adb ? "1" : "0");
    color(self, WIFI_ID, settings.wifi_adb ? 0x8eb5ff : 0xd8deeb);
    const char *ip = debug_ip();
    label(self, STATUS_ID, debug_status());
    char address[40] = "";
    if (settings.wifi_adb && *ip) {
        unsigned int n = append(address, 0, ip, sizeof(address));
        append(address, n, ":5555", sizeof(address));
    }
    label(self, ADDRESS_ID, address);
    unsigned int enabled = 0;
    for (unsigned int i = 0; i < settings.count; ++i) enabled += !!settings.patches[i].enabled;
    char counts[64] = "";
    unsigned int n = number(counts, 0, enabled);
    n = append(counts, n, " enabled / ", sizeof(counts));
    n = number(counts, n, settings.count);
    append(counts, n, settings.count > 5 ? " patches  -  Swipe to scroll" : " patches", sizeof(counts));
    label(self, COUNT_ID, counts);
    visible(self, EMPTY_ID, settings.count == 0);
    visible(self, LIST_ID, settings.count != 0);
    label(self, NOTICE_ID, launch_error ? launch_error : settings_ok ? "" : "Could not save settings to microSD");
}
/* Independent C adapters for the runtime's list interfaces. The first two
   virtual slots are destructors; the installed ARM ABI dispatches count at
   slot 2, row data at slot 3, and clicks at listener slot 2. */
static void list_noop(void *self) { (void)self; }
static int list_count(void *self, void *list) {
    (void)self; (void)list; return (int)settings.count;
}
static void list_data(void *self, void *list, void *item, int index) {
    (void)self; (void)list;
    if (index < 0 || (unsigned int)index >= settings.count) return;
    struct patch_entry *p = &settings.patches[index];
    set_text(item, p->title);
    set_color(item, p->compatible ? 0xe6e9f1 : 0x657080);
    void *detail = find_subitem(item, DETAIL_ID), *toggle = find_subitem(item, TOGGLE_ID);
    if (detail) set_text(detail, p->compatible ? p->description : "Not compatible with this installed app");
    if (toggle) set_text(toggle, !p->compatible ? "2" : p->enabled ? "1" : "0");
}
static void list_click(void *self, void *list, int index, int subitem) {
    (void)self; (void)subitem;
    if (phase != 1 || section != 0 || !list_owner || index < 0 || (unsigned int)index >= settings.count) return;
    struct patch_entry *p = &settings.patches[index];
    if (!p->compatible) return;
    p->enabled = !p->enabled;
    launch_error = (const char *)0;
    settings_ok = state_save(&settings);
    refresh_list(list);
    render(list_owner);
}
static void *adapter_table[] = {0, 0, (void *)list_noop, (void *)list_noop, (void *)list_count, (void *)list_data};
static void *listener_table[] = {0, 0, (void *)list_noop, (void *)list_noop, (void *)list_click};
static void **adapter = adapter_table + 2, **list_listener = listener_table + 2;

static void launch(void *self) {
    if (phase == 2 || phase == 3) return;
    if (!extension_recovery_ok) {
        launch_error = "Extension cleanup failed. See loader.log before retrying.";
        phase = 1; render(self);
        return;
    }
    launch_error = (const char *)0;
    if (!state_save(&settings)) { settings_ok = 0; phase = 1; render(self); return; }
    phase = 2; unregister_timer(self, BOOT_TIMER);
    label(self, NOTICE_ID, "Applying selected patches...");
    int result = state_apply(&settings, log_line);
    if (result < 0) {
        launch_error = "Patch could not be applied. Uncheck it to continue.";
        phase = 1; render(self);
        return;
    }
    const char *extensions[MAX_PATCHES+1]; unsigned int extension_count = 0;
    if (extension_probe("extra-menu")) extensions[extension_count++] = "extra-menu";
    for (unsigned int i = 0; i < settings.count; ++i)
        if (settings.patches[i].enabled && settings.patches[i].native)
            extensions[extension_count++] = settings.patches[i].id;
    int activated = extensions_start(extensions, extension_count, stock, runtime,
                                    result > 0 ? "/res/ui/dash1act.ftu" : (const char *)0, log_line);
    if (activated < 0) {
        state_restore();
        extension_recovery_ok = activated != -2;
        launch_error = extension_recovery_ok ?
              "Extension failed to activate. Uncheck it to continue." :
              "Extension cleanup failed. See loader.log before retrying.";
        phase = 1; render(self);
        return;
    }
    phase = 3; unregister_timer(self, NETWORK_TIMER);
    debug_release();
    log_line("official-selected\n");
    open_activity(ui_instance(), extensions_activity(stock_screen), (void *)0);
}
static int on_timer(void *self, int id) {
    if (id == BOOT_TIMER && phase == 0) { launch(self); return 1; }
    /* Only configuration owns a developer Wi-Fi connection. Never compete
       with the official app's hotspot or CarPlay/Android Auto networking. */
    if (id == NETWORK_TIMER && (phase == 1 || (phase == 0 && !settings.wifi_adb))) {
        debug_poll();
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
    phase = 0; section = 0; launch_error = (const char *)0;
    list_owner = self;
    void *list = find_control(self, LIST_ID);
    if (list) {
        set_adapter(list, &adapter); set_list_listener(list, &list_listener);
        refresh_list(list);
    }
    /* A one-shot deployment marker opens configuration for remote verification. */
    if (access("/mnt/extsd/Dashborne/config.once", 0) == 0) {
        extern int unlink(const char *);
        (void)unlink("/mnt/extsd/Dashborne/config.once"); phase = 1;
    }
    if (phase == 1 || !settings.wifi_adb) debug_set_enabled(settings.wifi_adb);
    render(self);
    register_timer(self, NETWORK_TIMER, 1000);
    if (!phase) register_timer(self, BOOT_TIMER, 3000);
    log_line(phase ? "configuration-created\n" : "splash-created\n");
}
static void on_click(void *self, void *control) {
    int id = control_id(control);
    if (id == TAP_ID && phase == 0) {
        phase = 1; unregister_timer(self, BOOT_TIMER); log_line("configuration-entered\n");
        debug_set_enabled(settings.wifi_adb);
    } else if (phase != 1) return;
    else if (id == LAUNCH_ID) { launch(self); return; }
    else if (id == PATCHES_TAB_ID) section = 0;
    else if (id == ADB_TAB_ID) section = 1;
    else if (id == WIFI_ID && section == 1) {
        settings.wifi_adb = !settings.wifi_adb;
        settings_ok = state_save(&settings);
        /* A failed save must never grant developer access. Disable immediately
           even if the card cannot persist that choice; keep the error visible. */
        if (!settings_ok && settings.wifi_adb) settings.wifi_adb = 0;
        debug_set_enabled(settings.wifi_adb);
    }
    render(self);
}

static void destroy(void *self) {
    if (list_owner == self) list_owner = (void *)0;
    activity_dtor(self); free(self);
}
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
    BIND(set_selected, "_ZN6ZKBase11setSelectedEb");
    BIND(set_adapter, "_ZN10ZKListView14setListAdapterEPNS_14AbsListAdapterE");
    BIND(set_list_listener, "_ZN10ZKListView20setItemClickListenerEPNS_18IItemClickListenerE");
    BIND(refresh_list, "_ZN10ZKListView15refreshListViewEv");
    BIND(find_subitem, "_ZNK10ZKListView10ZKListItem15findSubItemByIDEi");
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
    debug_bind(stock, runtime, log_line);
    return 1;
}

EXPORT void onEasyUIInit(void *ctx) {
    log_line("startup\n");
    extension_recovery_ok = extensions_recover(log_line);
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
    /* Leave the init-owned daemon alone, but cancel all pending work. */
    debug_release();
    (void)extensions_stop();
    if (prepared) { destroy(prepared); prepared = (void *)0; }
    log_line("shutdown\n");
    if (stock_deinit) stock_deinit(ctx);
}
