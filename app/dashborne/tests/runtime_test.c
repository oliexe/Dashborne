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
#include "../loader.c"
#undef calloc
#undef free

static const char *scenario;
static int init_calls, start_calls, deinit_calls, create_calls, dtor_calls;
static int allocations, frees, reads;
static void *(*registered)(void);
static void *original_table[17];
static char status_label[128], address[32];
static int timer_calls, wifi_polls, adb_polls;
static int opened, applied, saved, stopped, retried, cancelled, list_refreshes;
static void *bound_adapter, *bound_listener;
struct mock_control { int id; char text[256]; int shown, selected; };
static struct mock_control controls[40];
static int control_count;
void state_load(struct settings *s) {
    *s = (struct settings){0}; s->count = 2;
    s->wifi_adb = !strcmp(scenario, "developer-auto") || !strcmp(scenario, "developer-tap");
    strcpy(s->patches[0].id, "badge"); strcpy(s->patches[0].title, "Badge");
    s->patches[0].compatible = 1; s->patches[0].enabled = 1;
    strcpy(s->patches[1].id, "incompatible");
    if (!strcmp(scenario, "list")) {
        s->count = 64;
        strcpy(s->patches[63].title, "Last patch");
        s->patches[63].compatible = 1;
    }
}
int state_save(const struct settings *s) { assert(s->count == 2 || s->count == 64); ++saved; return strcmp(scenario, "save-failure") != 0; }
int state_apply(const struct settings *s, void (*log)(const char *)) {
    assert((s->count == 2 || s->count == 64) && log); ++applied; return !strcmp(scenario, "patch-failure") ? -1 : 1;
}
void tcp_adb_stop(void) { ++stopped; }
static const char *ip = "";
void tcp_adb_bind(void *p, void (*logger)(const char *)) { assert(p == (void *)2 && logger); }
void tcp_adb_poll(int network_ready) { assert(network_ready == !!*ip); ++adb_polls; }
const char *tcp_adb_status(void) { return "Wi-Fi ADB ready"; }
void wifi_debug_bind(void *p, void (*logger)(const char *)) { assert(p == (void *)1 && logger); }
void wifi_debug_poll(void) { ++wifi_polls; }
void wifi_debug_retry(void) { ++retried; }
const char *wifi_debug_status(void) { return "Wi-Fi: connecting..."; }
const char *wifi_debug_ip(void) { return ip; }
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
int fake_access(const char *path, int mode) { if (mode == 0) return -1; assert(!strcmp(path, LAYOUT) && mode == 4); return is("missing-layout") ? -1 : 0; }
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
    if (is("list") || is("developer-auto") || is("developer-tap") || is("normal") || is("tapped") || is("toggles") || is("patch-failure") || is("save-failure") || is("shutdown-prepared")) {
        assert(!strcmp(selected, ACTIVITY) && registered && create_calls == 1);
        assert(phase == 0 && timer_calls == 2 && !opened && !retried);
        if (!is("shutdown-prepared")) {
            void *p = registered();
            assert(!prepared && allocations == 1);
            void **v = *(void ***)p;
            void (*click)(void *, void *) = (void (*)(void *, void *))v[2];
            int (*tick)(void *, int) = (int (*)(void *, int))v[5];
            for (int i = 0; i < 120; ++i) tick(p, NETWORK_TIMER);
            assert(!retried && !wifi_polls && !adb_polls);
            if (is("list") || is("developer-tap") || is("tapped") || is("toggles")) {
                click(p, find(p, TAP_ID));
                assert(phase == 1 && cancelled == 1);
                if (is("developer-tap")) {
                    assert(retried == 1);
                    tick(p, NETWORK_TIMER);
                    assert(wifi_polls == 1 && adb_polls == 1);
                }
                tick(p, BOOT_TIMER); assert(!opened && !applied);
                if (is("toggles")) {
                    click(p, find(p, ADB_TAB_ID)); assert(section == 1);
                    click(p, find(p, WIFI_ID)); assert(settings.wifi_adb && retried == 1);
                    ip = "192.0.2.42"; tick(p, NETWORK_TIMER);
                    assert(!strcmp(address, "192.0.2.42:5555") && adb_polls == 1);
                    click(p, find(p, WIFI_ID)); assert(!settings.wifi_adb && stopped == 2);
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
            if (is("save-failure") || is("patch-failure")) assert(phase == 1 && !opened);
            else {
                assert(phase == 3 && opened == 1 && applied == 1);
                tick(p, BOOT_TIMER); click(p, find(p, LAUNCH_ID)); assert(opened == 1);
                int retries_before = retried, polls_before = wifi_polls, adb_before = adb_polls;
                for (int i = 0; i < 200; ++i) tick(p, NETWORK_TIMER);
                assert(retried == retries_before && wifi_polls == polls_before && adb_polls == adb_before);
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
