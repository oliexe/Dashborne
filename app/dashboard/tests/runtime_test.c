#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define RUNTIME_SIZE 7u
#define RUNTIME_HASH 0x44c0392bu
#define dlopen fake_dlopen
#define dlsym fake_dlsym
#define open fake_open
#define read fake_read
#define write fake_write
#define close fake_close
#define access fake_access
#define calloc fake_calloc
#define free fake_free
#define fsync fake_fsync
#include "../dashboard.c"
#undef calloc
#undef free

static const char *scenario;
static int init_calls, start_calls, deinit_calls, create_calls, dtor_calls;
static int allocations, frees, reads, opens;
static void *(*registered)(void);
static void *original_table[17];
static char label[32];
static int counter_control;
static int debug_control, debug_button, debug_toggles, timer_calls;
void usb_debug_bind(void *p, void (*logger)(const char *)) { assert(p == (void *)2 && logger); }
void wifi_debug_bind(void *p, void (*logger)(const char *)) { assert(p == (void *)1 && logger); }
void wifi_debug_poll(void) {}
void wifi_debug_retry(void) {}
const char *wifi_debug_status(void) { return "Wi-Fi: ready"; }
const char *wifi_debug_ip(void) { return ""; }
void usb_debug_toggle_wifi(void) {}
int usb_debug_wifi_owned(void) { return 0; }
void usb_debug_toggle(void) { ++debug_toggles; }
void usb_debug_shutdown(void) { }
void usb_debug_collect(void) { }
void usb_debug_poll(void) { }
const char *usb_debug_status(void) { return "ADB: off"; }
int usb_debug_owned(void) { return 0; }
static void timer(void *p, int id, int interval) {
    assert(p && id == DEBUG_TIMER && interval == 1000); ++timer_calls;
}
static int is(const char *s) { return !strcmp(scenario, s); }
static void original_init(void *p) { assert(p == (void *)123); ++init_calls; }
static void original_deinit(void *p) { assert(p == (void *)123); ++deinit_calls; }
static const char *original_start(void *p) { assert(p == (void *)123); ++start_calls; return "installedActivity"; }
static void *factory(void) { return (void *)456; }
static void reg_activity(void *f, const char *n, void *(*fn)(void)) {
    assert(init_calls == 1 && f == (void *)456 && !strcmp(n, ACTIVITY)); registered = fn;
}
static void ctor(void *p) { ((void **)p)[0] = original_table + 2; ((void **)p)[1] = original_table + 14; }
static void dtor(void *p) { assert(p); ++dtor_calls; }
static void base_created(void *p) { assert(p); ++create_calls; }
static const char *base_name(void *p) { (void)p; return "base"; }
static int create(void *p) {
    if (is("create-failure")) return 0;
    void **v = *(void ***)p;
    assert(!strcmp(((const char *(*)(void *))v[3])(p), "../../../mnt/extsd/ui/dashborne.ftu"));
    ((void (*)(void *))v[4])(p);
    return 1;
}
static void *find(void *p, int id) {
    assert(p);
    if (id == COUNT_ID) return &counter_control;
    if (id == DEBUG_STATUS_ID) return &debug_control;
    if (id == WIFI_ID || id == WIFI_STATUS_ID) return NULL;
    assert(id == DEBUG_ID); return &debug_button;
}
static int get_id(void *p) { return *(int *)p; }
static void text(void *p, const char *s) {
    if (p == &counter_control) snprintf(label, sizeof(label), "%s", s);
    else assert(p == &debug_control || p == &debug_button);
}
static void navigate(void *p, const char *s, void *intent) {
    assert(p == (void *)123 && !strcmp(s, "installedActivity") && !intent); ++opens;
}
void *fake_dlopen(const char *path, int flags) {
    assert(flags == 1);
    if (!strcmp(path, "/res/lib/libzkgui.so")) return (void *)1;
    assert(!strcmp(path, "/lib/libeasyui.so")); return (void *)2;
}
void *fake_dlsym(void *handle, const char *name) {
    if (handle == (void *)1) {
        if (!strcmp(name, "onEasyUIInit")) return (void *)original_init;
        if (!strcmp(name, "onEasyUIDeinit")) return (void *)original_deinit;
        if (!strcmp(name, "onStartupApp")) return (void *)original_start;
        assert(0);
    }
    assert(handle == (void *)2);
    if (is("missing-symbol") && !strcmp(name, "_ZN8ActivityC1Ev")) return NULL;
    struct { const char *name; void *address; } symbols[] = {
        {"_ZN15ActivityFactory11getInstanceEv", (void *)factory},
        {"_ZN15ActivityFactory16registerActivityEPKcPFP8ActivityvE", (void *)reg_activity},
        {"_ZN8ActivityC1Ev", (void *)ctor}, {"_ZN8ActivityD1Ev", (void *)dtor},
        {"_ZN7BaseApp6createEv", (void *)create}, {"_ZN7BaseApp8onCreateEv", (void *)base_created},
        {"_ZN7BaseApp15findControlByIDEi", (void *)find}, {"_ZNK6ZKBase5getIDEv", (void *)get_id},
        {"_ZN10ZKTextView7setTextEPKc", (void *)text},
        {"_ZN13EasyUIContext12openActivityEPKcP6Intent", (void *)navigate},
        {"_ZN7BaseApp13registerTimerEii", (void *)timer},
        {"_ZTV8Activity", original_table}, {"_ZNK7BaseApp10getAppNameEv", (void *)base_name}
    };
    for (unsigned i = 0; i < sizeof(symbols) / sizeof(symbols[0]); ++i)
        if (!strcmp(name, symbols[i].name)) return symbols[i].address;
    assert(0); return NULL;
}
int fake_open(const char *path, int flags, ...) {
    if (flags == 0) { assert(!strcmp(path, "/lib/libeasyui.so")); return 10; }
    assert(!strcmp(path, "/mnt/extsd/Dashborne/events.log")); return 11;
}
int fake_read(int fd, void *buf, unsigned int count) {
    assert(fd == 10 && count >= 7);
    if (is("read-error")) return -1;
    if (reads++) return 0;
    memcpy(buf, is("wrong-runtime") ? "changed" : "runtime", 7); return 7;
}
int fake_write(int fd, const void *buf, unsigned int count) { assert(fd == 11 && buf); return (int)count; }
int fake_close(int fd) { assert(fd == 10 || fd == 11); return 0; }
int fake_fsync(int fd) { assert(fd == 11); return 0; }
int fake_access(const char *path, int mode) { assert(!strcmp(path, LAYOUT) && mode == 4); return is("missing-layout") ? -1 : 0; }
void *fake_calloc(usize n, usize size) {
    if (is("allocation-failure")) return NULL;
    ++allocations; return calloc(n, size);
}
void fake_free(void *p) { assert(p); ++frees; free(p); }

int main(int argc, char **argv) {
    assert(argc == 2); scenario = argv[1];
    original_table[5] = (void *)base_name;
    original_table[6] = (void *)base_created;
    original_table[12] = (void *)-(__PTRDIFF_TYPE__)sizeof(void *);
    if (is("wrong-vtable")) original_table[6] = NULL;
    onEasyUIInit((void *)123);
    const char *selected = onStartupApp((void *)123);
    assert(init_calls == 1 && start_calls == 1);
    if (is("normal") || is("shutdown-prepared")) {
        assert(!strcmp(selected, ACTIVITY) && registered && create_calls == 1);
        assert(!strcmp(label, "Touches: 0"));
        if (is("normal")) {
            void *p = registered();
            assert(!prepared && allocations == 1);
            void **v = *(void ***)p;
            int tap = TAP_ID, stock_button = STOCK_ID, other = 999, debug = DEBUG_ID;
            assert(timer_calls == 1);
            ((void (*)(void *, void *))v[2])(p, &debug);
            assert(debug_toggles == 1);
            assert(((int (*)(void *, int))v[5])(p, DEBUG_TIMER) == 1);
            void *secondary = (void **)p + 1;
            void **sv = *(void ***)secondary;
            assert(((int (*)(void *, int))sv[2])(secondary, DEBUG_TIMER) == 1);
            for (int i = 0; i < 12; ++i) ((void (*)(void *, void *))v[2])(p, &tap);
            assert(!strcmp(label, "Touches: 12"));
            ((void (*)(void *, void *))v[2])(p, &other);
            assert(!strcmp(label, "Touches: 12"));
            ((void (*)(void *, void *))v[2])(p, &stock_button);
            assert(opens == 1);
            ((void (*)(void *))v[1])(p);
        }
    } else {
        assert(!strcmp(selected, "installedActivity"));
        assert(!prepared);
    }
    onEasyUIDeinit((void *)123);
    assert(deinit_calls == 1 && allocations == frees && dtor_calls == frees);
    printf("PASS: %s\n", scenario);
}
