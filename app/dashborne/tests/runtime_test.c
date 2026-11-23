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
#define unlink fake_unlink
#define calloc fake_calloc
#define free fake_free
#define fsync fake_fsync
#include "../loader.c"
#undef calloc
#undef free

static const char *scenario;
static int init_calls, start_calls, deinit_calls, create_calls, dtor_calls;
static int allocations, frees, reads;
static void *(*registered)(void);
static void *original_table[17];
static char status_label[128], address[32];
static int timer_calls, adb_polls, released;
static int opened, applied, saved, stopped, retried, cancelled, list_refreshes;
static void *bound_adapter, *bound_listener;
static int force_save_failure, extension_calls, restored;
struct mock_control { int id; char text[256]; int shown, selected; };
static struct mock_control controls[40];
static int control_count;
void state_restore(void) { ++restored; }
void state_load(struct settings *s) {
    *s = (struct settings){0}; s->count = 2;
    s->wifi_adb = !strcmp(scenario, "developer-auto") || !strcmp(scenario, "developer-tap") || !strcmp(scenario, "config-on");
    strcpy(s->patches[0].id, "badge"); strcpy(s->patches[0].title, "Badge");
    s->patches[0].compatible = 1; s->patches[0].enabled = 1;
    strcpy(s->patches[1].id, "incompatible");
    if (!strcmp(scenario, "extension") || !strcmp(scenario, "extension-failure") || !strcmp(scenario, "extension-cleanup-failure")) {
        strcpy(s->patches[1].id, "native-example");
        s->patches[1].native = s->patches[1].enabled = s->patches[1].compatible = 1;
    }
    if (!strcmp(scenario, "list")) {
        s->count = 64;
        strcpy(s->patches[63].title, "Last patch");
        s->patches[63].compatible = 1;
    }
}
int state_save(const struct settings *s) { assert(s->count == 2 || s->count == 64); ++saved; return !force_save_failure && strcmp(scenario, "save-failure") != 0; }
int state_apply(const struct settings *s, void (*log)(const char *)) {
    assert((s->count == 2 || s->count == 64) && log); ++applied; return !strcmp(scenario, "patch-failure") ? -1 : 1;
}
int extension_probe(const char *id) { assert(!strcmp(id,"extra-menu")); return !strcmp(scenario,"extension"); }
int extensions_recover(void (*logger)(const char *)) { assert(logger); return 1; }
int extensions_start(const char *const *ids, unsigned int count, void *s, void *r,
                     const char *protected_target, void (*logger)(const char *)) {
    assert(ids && s && r && protected_target && logger); ++extension_calls;
    if (!strcmp(scenario,"extension")) { assert(count==2 && !strcmp(ids[0],"extra-menu") && !strcmp(ids[1],"native-example")); }
    else if (settings.patches[1].native) { assert(count == 1 && !strcmp(ids[0], "native-example")); }
    else assert(!count);
    if (!strcmp(scenario, "extension-failure")) return -1;
    if (!strcmp(scenario, "extension-cleanup-failure")) return -2;
    return (int)count;
}
const char *extensions_activity(const char *name) { return name; }
int extensions_stop(void) { return 1; }
static const char *ip = "";
void debug_bind(void *s, void *r, void (*logger)(const char *)) { assert(s == (void *)1 && r == (void *)2 && logger); }
void debug_poll(void) { ++adb_polls; }
void debug_set_enabled(int enabled) { if (enabled) ++retried; else ++stopped; }
void debug_release(void) { ++released; }
const char *debug_status(void) { return "ADB listening"; }
const char *debug_ip(void) { return ip; }
static void timer(void *p, int id, int interval) {
    assert(p && ((id == NETWORK_TIMER && interval == 1000) || (id == BOOT_TIMER && interval == 3000))); ++timer_calls;
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
    assert(!strcmp(((const char *(*)(void *))v[3])(p), "../../../mnt/extsd/Dashborne/ui/loader.ftu"));
    ((void (*)(void *))v[4])(p);
    return 1;
}
static void *find(void *p, int id) {
    assert(p);
    for (int i = 0; i < control_count; ++i) if (controls[i].id == id) return &controls[i];
    assert(control_count < 40); controls[control_count].id = id;
    return &controls[control_count++];
}
static void attach_adapter(void *p, void *a) { assert(p && a); bound_adapter = a; }
static void attach_listener(void *p, void *a) { assert(p && a); bound_listener = a; }
static void refresh(void *p) { assert(p); ++list_refreshes; }
static void *subitem(void *p, int id) { return find(p, id); }
static void item_click(void *p, int index) {
    void **v = *(void ***)bound_listener;
    ((void (*)(void *, void *, int, int))v[2])(bound_listener, find(p, LIST_ID), index, TOGGLE_ID);
}
static void obtain(void *p, int index) {
    void **v = *(void ***)bound_adapter;
    assert(((int (*)(void *, void *))v[2])(bound_adapter, find(p, LIST_ID)) == (int)settings.count);
    ((void (*)(void *, void *, void *, int))v[3])(bound_adapter, find(p, LIST_ID), find(p, 24000), index);
}
static void text(void *p, const char *s) {
    struct mock_control *c = p; snprintf(c->text, sizeof(c->text), "%s", s);
    if (c->id == STATUS_ID) snprintf(status_label, sizeof(status_label), "%s", s);
    if (c->id == ADDRESS_ID) snprintf(address, sizeof(address), "%s", s);
}
static void visibility(void *p, int shown) { ((struct mock_control *)p)->shown = shown; }
static void selection(void *p, int selected) { ((struct mock_control *)p)->selected = selected; }
static void text_color(void *p, int rgb) { assert(p && rgb); }
static int id_of(void *p) { return ((struct mock_control *)p)->id; }
static void cancel_timer(void *p, int id) { assert(p && (id == BOOT_TIMER || id == NETWORK_TIMER)); ++cancelled; }
static void *ui(void) { return (void *)789; }
static void navigate(void *p, const char *activity, void *intent) {
    assert(p == (void *)789 && !strcmp(activity, "installedActivity") && !intent); ++opened;
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
    if (is("missing-list-api") && !strcmp(name, "_ZN10ZKListView14setListAdapterEPNS_14AbsListAdapterE")) return NULL;
    struct { const char *name; void *address; } symbols[] = {
        {"_ZN15ActivityFactory11getInstanceEv", (void *)factory},
        {"_ZN15ActivityFactory16registerActivityEPKcPFP8ActivityvE", (void *)reg_activity},
        {"_ZN8ActivityC1Ev", (void *)ctor}, {"_ZN8ActivityD1Ev", (void *)dtor},
        {"_ZN7BaseApp6createEv", (void *)create}, {"_ZN7BaseApp8onCreateEv", (void *)base_created},
        {"_ZN7BaseApp15findControlByIDEi", (void *)find},
        {"_ZN10ZKTextView7setTextEPKc", (void *)text},
        {"_ZN7BaseApp13registerTimerEii", (void *)timer},
        {"_ZN7BaseApp15unregisterTimerEi", (void *)cancel_timer},
        {"_ZNK6ZKBase5getIDEv", (void *)id_of},
        {"_ZN6ZKBase11setSelectedEb", (void *)selection},
        {"_ZN6ZKBase10setVisibleEb", (void *)visibility},
        {"_ZN10ZKTextView12setTextColorEi", (void *)text_color},
        {"_ZN10ZKListView14setListAdapterEPNS_14AbsListAdapterE", (void *)attach_adapter},
        {"_ZN10ZKListView20setItemClickListenerEPNS_18IItemClickListenerE", (void *)attach_listener},
        {"_ZN10ZKListView15refreshListViewEv", (void *)refresh},
        {"_ZNK10ZKListView10ZKListItem15findSubItemByIDEi", (void *)subitem},
        {"_ZN13EasyUIContext11getInstanceEv", (void *)ui},
        {"_ZN13EasyUIContext12openActivityEPKcP6Intent", (void *)navigate},
        {"_ZTV8Activity", original_table}, {"_ZNK7BaseApp10getAppNameEv", (void *)base_name}
    };
    for (unsigned i = 0; i < sizeof(symbols) / sizeof(symbols[0]); ++i)
        if (!strcmp(name, symbols[i].name)) return symbols[i].address;
    assert(0); return NULL;
}
int fake_open(const char *path, int flags, ...) {
    if (flags == 0) { assert(!strcmp(path, "/lib/libeasyui.so")); return 10; }
    assert(!strcmp(path, "/mnt/extsd/Dashborne/loader.log")); return 11;
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
int fake_unlink(const char *path) { assert(!strcmp(path, "/mnt/extsd/Dashborne/config.once")); return 0; }
int fake_access(const char *path, int mode) { if (mode == 0) return (is("config-on") || is("config-off")) ? 0 : -1; assert(!strcmp(path, LAYOUT) && mode == 4); return is("missing-layout") ? -1 : 0; }
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
    if (is("extension") || is("extension-failure") || is("extension-cleanup-failure") || is("config-on") || is("config-off") || is("list") || is("developer-auto") || is("developer-tap") || is("normal") || is("tapped") || is("toggles") || is("patch-failure") || is("save-failure") || is("shutdown-prepared")) {
        assert(!strcmp(selected, ACTIVITY) && registered && create_calls == 1);
        int marked = is("config-on") || is("config-off");
        assert(phase == marked && timer_calls == (marked ? 1 : 2) && !opened);
        assert(retried == (is("config-on") ? 1 : 0));
        if (!is("shutdown-prepared")) {
            void *p = registered();
            assert(!prepared && allocations == 1);
            void **v = *(void ***)p;
            void (*click)(void *, void *) = (void (*)(void *, void *))v[2];
            int (*tick)(void *, int) = (int (*)(void *, int))v[5];
            for (int i = 0; i < 120; ++i) tick(p, NETWORK_TIMER);
            assert(retried == (is("config-on") ? 1 : 0));
            assert(adb_polls == ((marked || !settings.wifi_adb) ? 120 : 0));
            adb_polls = 0;
            if (marked || is("list") || is("developer-tap") || is("tapped") || is("toggles")) {
                if (!marked) click(p, find(p, TAP_ID));
                assert(phase == 1 && cancelled == (marked ? 0 : 1));
                if (is("developer-tap")) {
                    assert(retried == 1);
                    tick(p, NETWORK_TIMER);
                    assert(adb_polls == 1);
                }
                tick(p, BOOT_TIMER); assert(!opened && !applied);
                if (is("toggles")) {
                    click(p, find(p, ADB_TAB_ID)); assert(section == 1);
                    force_save_failure = 1;
                    click(p, find(p, WIFI_ID)); assert(!settings.wifi_adb && !retried && !settings_ok);
                    force_save_failure = 0;
                    click(p, find(p, WIFI_ID)); assert(settings.wifi_adb && retried == 1);
                    ip = "192.0.2.42"; tick(p, NETWORK_TIMER);
                    assert(!strcmp(address, "192.0.2.42:5555") && adb_polls == 1);
                    force_save_failure = 1;
                    click(p, find(p, WIFI_ID)); assert(!settings.wifi_adb && stopped == 4 && !settings_ok);
                    force_save_failure = 0;
                    click(p, find(p, PATCHES_TAB_ID)); assert(section == 0);
                    item_click(p, 0); assert(!settings.patches[0].enabled);
                    item_click(p, 1); assert(!settings.patches[1].enabled);
                }
                if (is("list")) {
                    assert(section == 0 && ((struct mock_control *)find(p, PATCHES_PANEL_ID))->shown);
                    int before_nav = saved;
                    click(p, find(p, ADB_TAB_ID));
                    assert(section == 1 && !((struct mock_control *)find(p, PATCHES_PANEL_ID))->shown);
                    assert(((struct mock_control *)find(p, ADB_PANEL_ID))->shown);
                    item_click(p, 0); assert(saved == before_nav);
                    click(p, find(p, PATCHES_TAB_ID)); assert(section == 0 && saved == before_nav);
                    obtain(p, 63);
                    assert(!strcmp(((struct mock_control *)find(p, 24000))->text, "Last patch"));
                    assert(!strcmp(((struct mock_control *)find(p, TOGGLE_ID))->text, "0"));
                    item_click(p, 63); assert(settings.patches[63].enabled && saved == 1);
                    obtain(p, 63); assert(!strcmp(((struct mock_control *)find(p, TOGGLE_ID))->text, "1"));
                    obtain(p, 1); assert(!strcmp(((struct mock_control *)find(p, TOGGLE_ID))->text, "2"));
                    item_click(p, 1); item_click(p, -1); item_click(p, 64); assert(saved == 1);
                    obtain(p, 0); assert(!strcmp(((struct mock_control *)find(p, 24000))->text, "Badge"));
                    int before_refresh = list_refreshes;
                    tick(p, NETWORK_TIMER); assert(list_refreshes == before_refresh);
                    item_click(p, 63); assert(!settings.patches[63].enabled && saved == 2);
                }
                click(p, find(p, LAUNCH_ID));
            } else tick(p, BOOT_TIMER);
            if (is("save-failure") || is("patch-failure") || is("extension-failure") || is("extension-cleanup-failure")) {
                assert(phase == 1 && !opened);
                if (is("extension-failure") || is("extension-cleanup-failure")) assert(restored == 1);
                if (is("extension-cleanup-failure")) {
                    click(p, find(p, LAUNCH_ID)); assert(extension_calls == 1);
                }
            }
            else {
                assert(phase == 3 && opened == 1 && applied == 1);
                tick(p, BOOT_TIMER); click(p, find(p, LAUNCH_ID)); assert(opened == 1);
                assert(released == 1);
                int retries_before = retried, adb_before = adb_polls;
                for (int i = 0; i < 200; ++i) tick(p, NETWORK_TIMER);
                assert(retried == retries_before && adb_polls == adb_before);
            }
            ((void (*)(void *))v[1])(p);
        }
    } else {
        assert(!strcmp(selected, "installedActivity") && !prepared);
    }
    onEasyUIDeinit((void *)123);
    assert(deinit_calls == 1 && allocations == frees && dtor_calls == frees);
    printf("PASS: %s\n", scenario);
}
