#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define RUNTIME_SIZE 7u
#define RUNTIME_HASH 0x44c0392bu
#define DASHBORNE_VERSION "9.8.7-test"
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
#include "../wifi_editor.c"
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
static struct mock_control controls[160];
static int control_count;
static char config_ssid[33]="TestNetwork",config_password[64]="testpassword";
static char pending_ssid[33],pending_password[64];
static int wifi_commits;
static int updater_active,updater_calls,wifi_connections;
static const char *updater_action;
static struct update_release test_release={"123","0.2.0","6.11.4","Test release notes",1};
int config_load(void) {strcpy(pending_ssid,config_ssid);strcpy(pending_password,config_password);return strcmp(scenario,"wifi-read-failure")!=0;}
int config_get(const char *key,char *out,unsigned int capacity) {
    snprintf(out,capacity,"%s",!strcmp(key,"wifi.ssid") ? pending_ssid : pending_password);return 1;
}
int config_set(const char *key,const char *value) {
    if(!strcmp(key,"wifi.ssid"))snprintf(pending_ssid,sizeof(pending_ssid),"%s",value);
    else if(!strcmp(key,"wifi.password"))snprintf(pending_password,sizeof(pending_password),"%s",value);
    else assert(0);
    return 1;
}
int config_commit(void) {
    if(force_save_failure)return 0;
    strcpy(config_ssid,pending_ssid);strcpy(config_password,pending_password);++wifi_commits;return 1;
}
void config_clear(void) {memset(pending_ssid,0,sizeof(pending_ssid));memset(pending_password,0,sizeof(pending_password));}
void state_restore(void) { ++restored; }
void state_load(struct settings *s) {
    *s = (struct settings){0}; s->count = 2;s->config_ok=strcmp(scenario,"config-invalid")!=0;
    if(!strncmp(scenario,"force-",6)){s->wifi_adb=s->adb_force=1;s->adb_auto_restart=strstr(scenario,"watchdog")!=NULL;}
    if(!strncmp(scenario,"autolaunch",10)){s->loader_autolaunch=1;strcpy(s->loader_app,"/mnt/extsd/test.so");}
    s->wifi_adb = !strncmp(scenario,"force-",6) || !strcmp(scenario, "developer-auto") || !strcmp(scenario, "developer-tap") || !strcmp(scenario, "config-on");
    strcpy(s->patches[0].id, "test-label"); strcpy(s->patches[0].title, "Test label");
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
    assert((s->count == 2 || s->count == 64) && log); ++applied; return !strcmp(scenario,"patch-failure") && s->patches[0].enabled ? -1 : 1;
}
unsigned int state_rejected_count(void){return !strcmp(scenario,"rejected-extension");}
const char *state_failed_id(void){return "test-label";}
const char *extensions_failed_id(void){return !strcmp(scenario,"core-failure")?"extra-menu":"native-example";}
int extension_probe(const char *id) { assert(!strcmp(id,"extra-menu") || !strcmp(id,"widget-runtime")); return !strcmp(scenario,"extension"); }
int extensions_recover(void (*logger)(const char *)) { assert(logger); return 1; }
int extensions_start(const char *const *ids, unsigned int count, void *s, void *r,
                     const char *protected_target, void (*logger)(const char *)) {
    assert(ids && s && r && protected_target && logger); ++extension_calls;
    if (settings.patches[1].native && settings.patches[1].enabled) { assert(count == 1 && !strcmp(ids[0], "native-example")); }
    else assert(!count);
    if (!strcmp(scenario,"extension-failure") && count) return -1;
    if (!strcmp(scenario,"core-failure"))return -1;
    if (!strcmp(scenario, "extension-cleanup-failure")) return -2;
    return (int)count;
}
const char *extensions_activity(const char *name) { return name; }
int extensions_stop(void) { return 1; }
static int device_reads;
void device_bind(void *s,void *r) {assert(s==(void *)1 && r==(void *)2);}
void device_refresh(void) {++device_reads;}
unsigned int device_count(void) {return 1;}
const struct device_row *device_row(unsigned int i) {
    static const struct device_row row={"System firmware","1.2.3"};return i==0 ? &row : NULL;
}
static int browser_prepared, browser_cancelled, browser_opens, restarts;
void browser_init(void) { ++browser_opens; }
void browser_refresh(void) {}
void browser_up(void) {}
void browser_select(unsigned int i) { (void)i; }
unsigned int browser_count(void) { return 0; }
const struct browser_entry *browser_entry(unsigned int i) { (void)i;return NULL; }
const char *browser_path(void) { return "/mnt/extsd"; }
const char *browser_selection(void) { return "/mnt/extsd/test.so"; }
const char *browser_status(void) { return "Select an app"; }
int browser_can_load(void) { return 1; }
int browser_prepare_path(const char *p) {assert(!strcmp(p,"/mnt/extsd/test.so"));++browser_prepared;return strcmp(scenario,"autolaunch-invalid")!=0;}
int browser_prepare(void) { ++browser_prepared;return strcmp(scenario,"browser-prepare-failure")!=0; }
void browser_cancel_request(void) { ++browser_cancelled; }
int browser_take_request(char *path) {
    if(!strcmp(scenario,"external-app") || !strcmp(scenario,"external-empty") || !strcmp(scenario,"external-load-failure") || !strcmp(scenario,"external-symbol-failure") || !strcmp(scenario,"external-self")) { strcpy(path,"/mnt/extsd/app.so");return 1; }
    return !strcmp(scenario,"external-invalid") ? -1 : 0;
}
int browser_take_return(void) { return !strcmp(scenario,"external-return") || !strcmp(scenario,"autolaunch-return"); }
static int restart_prop(const char *key,const char *value) {
    assert(!strcmp(key,"ctl.restart") && !strcmp(value,"zkswe"));++restarts;
    return (!strcmp(scenario,"browser-restart-failure") || !strcmp(scenario,"autolaunch-restart-failure")) ? -1 : 0;
}
static int forced_debug, watch_started, watch_beats;
int debug_set_forced(int value) {forced_debug=value;return 1;}
int debug_is_forced(void) {return forced_debug;}
int watchdog_start(void) {++watch_started;return 1;}
int watchdog_ready(void) {return strcmp(scenario,"force-watchdog-fail") && watch_started!=0;}
void watchdog_beat(void) {++watch_beats;}
void watchdog_stop(void) {watch_started=0;}
int watchdog_recovery(void) {return 0;}
static const char *ip = "";
void debug_bind(void *s, void *r, void (*logger)(const char *)) { assert(s == (void *)1 && r == (void *)2 && logger); }
void debug_poll(void) { ++adb_polls; }
void debug_set_enabled(int enabled) { if (enabled) ++retried; else {++stopped;forced_debug=0;} }
void debug_release(void) { forced_debug=0;++released; }
const char *debug_status(void) { return "ADB listening"; }
const char *debug_ip(void) { return ip; }
const char *debug_wifi_name(void) { return "TestNetwork"; }
static void timer(void *p, int id, int interval) {
    assert(p && ((id == NETWORK_TIMER && interval == 1000) || (id == BOOT_TIMER && interval == 3000) || (id == LOADING_TIMER && interval == 180) || (id==CONTINUE_TIMER && interval==2000))); ++timer_calls;
}
static int is(const char *s) { return !strcmp(scenario, s); }
static void original_init(void *p) { assert(p == (void *)123); ++init_calls; }
static void original_deinit(void *p) { assert(p == (void *)123); ++deinit_calls; }
static const char *original_start(void *p) { assert(p == (void *)123); ++start_calls; return !strcmp(scenario,"external-empty") ? "" : "installedActivity"; }
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
    assert(control_count < 160); controls[control_count].id = id;
    return &controls[control_count++];
}
static void attach_adapter(void *p, void *a) { assert(p && a); if (((struct mock_control *)p)->id == LIST_ID) bound_adapter = a; }
static void attach_listener(void *p, void *a) { assert(p && a); if (((struct mock_control *)p)->id == LIST_ID) bound_listener = a; }
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
static void dirty(void *p,const void *rect) {assert(p && !rect);}
static void selection(void *p, int selected) { ((struct mock_control *)p)->selected = selected; }
static void text_color(void *p, int rgb) { assert(p && rgb); }
static int id_of(void *p) { return ((struct mock_control *)p)->id; }
static void cancel_timer(void *p, int id) { assert(p && (id == BOOT_TIMER || id == NETWORK_TIMER || id == LOADING_TIMER)); ++cancelled; }
static void *ui(void) { return (void *)789; }
static void navigate(void *p, const char *activity, void *intent) {
    assert(p == (void *)789 && !strcmp(activity, "installedActivity") && !intent); ++opened;
}
void *fake_dlopen(const char *path, int flags) {
    assert(flags == 1);
    if (!strcmp(path, "/res/lib/libzkgui.so")) return (void *)1;
    if (!strcmp(path, "/mnt/extsd/app.so")) return is("external-load-failure") ? NULL : (void *)3;
    assert(!strcmp(path, "/lib/libeasyui.so")); return (void *)2;
}
void *fake_dlsym(void *handle, const char *name) {
    if(handle == (void *)3 && is("external-symbol-failure") && !strcmp(name,"onStartupApp")) return NULL;
    if(handle == (void *)3 && is("external-self") && !strcmp(name,"onEasyUIInit")) return (void *)onEasyUIInit;
    if (handle == (void *)1 || handle == (void *)3) {
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
        {"_ZN6ZKBase10invalidateEPK14LayoutPosition", (void *)dirty},
        {"_ZN6ZKBase11setSelectedEb", (void *)selection},
        {"_ZN6ZKBase10setVisibleEb", (void *)visibility},
        {"_ZN10ZKTextView12setTextColorEi", (void *)text_color},
        {"_ZN10ZKListView14setListAdapterEPNS_14AbsListAdapterE", (void *)attach_adapter},
        {"_ZN10ZKListView20setItemClickListenerEPNS_18IItemClickListenerE", (void *)attach_listener},
        {"_ZN10ZKListView15refreshListViewEv", (void *)refresh},
        {"_ZNK10ZKListView10ZKListItem15findSubItemByIDEi", (void *)subitem},
        {"_ZN13EasyUIContext11getInstanceEv", (void *)ui},
        {"_ZN13EasyUIContext12openActivityEPKcP6Intent", (void *)navigate},
        {"_ZN16SystemProperties9setStringEPKcS1_", (void *)restart_prop},
        {"_ZTV8Activity", original_table}, {"_ZNK7BaseApp10getAppNameEv", (void *)base_name}
    };
    for (unsigned i = 0; i < sizeof(symbols) / sizeof(symbols[0]); ++i)
        if (!strcmp(name, symbols[i].name)) return symbols[i].address;
    assert(0); return NULL;
}
int fake_open(const char *path, int flags, ...) {
    if (flags == 0) { assert(!strcmp(path, "/lib/libeasyui.so")); return 10; }
    assert(!strcmp(path,"/mnt/extsd/Dashborne/loader.log") || !strcmp(path,STOCK_ONCE)); return 11;
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
int fake_access(const char *path, int mode) { if(!strcmp(path,STOCK_ONCE))return -1; if (mode == 0) return (is("config-on") || is("config-off")) ? 0 : -1; assert(!strcmp(path, LAYOUT) && mode == 4); return is("missing-layout") ? -1 : 0; }
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
    if(is("updates")) {
        void *p=registered();on_click(p,find(p,TAP_ID));on_click(p,find(p,20018));
        assert(section==4 && !settings.wifi_adb);
        on_click(p,find(p,20300));assert(wifi_form.active);on_click(p,find(p,WIFI_SAVE_ID));
        assert(!wifi_form.active && wifi_connections==1 && !retried && !settings.wifi_adb);
        on_click(p,find(p,20301));assert(update_waiting && wifi_connections==2);
        on_click(p,find(p,20301));on_click(p,find(p,20303));release_click(NULL,find(p,80004),0,0);
        assert(wifi_connections==2 && update_selection==-1);
        assert(!strcmp(((struct mock_control *)find(p,50207))->text,"Connecting to Wi-Fi..."));
        ip="192.0.2.42";on_timer(p,NETWORK_TIMER);assert(!update_waiting && updater_calls==1 && !strcmp(updater_action,"list"));
        int refreshes=list_refreshes;on_timer(p,NETWORK_TIMER);assert(list_refreshes==refreshes);
        release_click(NULL,find(p,80004),0,0);assert(update_selection==0 && update_confirming);
        on_click(p,find(p,LAUNCH_ID));assert(!opened && update_confirming);
        on_click(p,find(p,20304));assert(!update_confirming && updater_calls==1);
        test_release.supported=0;release_click(NULL,find(p,80004),0,0);
        assert(!update_confirming && updater_calls==1);
        on_click(p,find(p,20302));assert(updater_calls==1);
        test_release.supported=1;
        release_click(NULL,find(p,80004),0,0);assert(update_confirming);
        on_click(p,find(p,20302));assert(!update_confirming && updater_calls==2 && !strcmp(updater_action,"install"));
        updater_active=1;on_click(p,find(p,LAUNCH_ID));on_click(p,find(p,ADB_TAB_ID));assert(!opened && section==4);
        updater_active=0;on_click(p,find(p,20303));assert(update_confirming);
        on_click(p,find(p,20302));assert(updater_calls==3 && !strcmp(updater_action,"rollback"));
        destroy(p);onEasyUIDeinit((void *)123);puts("PASS: updates share Wi-Fi, require confirmation and block conflicting actions");return 0;
    }
    if(!strncmp(scenario,"wifi-",5)) {
        void *p=registered();on_click(p,find(p,TAP_ID));on_click(p,find(p,ADB_TAB_ID));
        assert(strstr(((struct mock_control *)find(p,50014))->text,DASHBORNE_VERSION));
        if(!is("wifi-read-failure")) {
            const int row_ids[]={NETWORK_EDIT_ID,NETWORK_NAME_ID,50076};
            for(unsigned int i=0;i<3;++i) {
                on_click(p,find(p,row_ids[i]));assert(wifi_form.active);
                on_click(p,find(p,WIFI_CANCEL_ID));assert(!wifi_form.active);
            }
        }
        on_click(p,find(p,NETWORK_EDIT_ID));
        if(is("wifi-read-failure")){assert(!wifi_form.active && launch_error);return 0;}
        assert(wifi_form.active && !((struct mock_control *)find(p,CONFIG_ID))->shown);
        assert(!strstr(((struct mock_control *)find(p,WIFI_PASSWORD_ID))->text,"testpassword"));
        on_click(p,find(p,LAUNCH_ID));assert(wifi_form.active && !opened);
        on_click(p,find(p,PATCHES_TAB_ID));assert(section==1);
        on_click(p,find(p,WIFI_CLEAR_ID));on_click(p,find(p,WIFI_KEY_ID+10));
        assert(!strcmp(wifi_form.ssid,"q"));
        int before=retried;
        on_click(p,find(p,WIFI_CANCEL_ID));assert(!wifi_form.active && !wifi_commits && retried==before);
        on_click(p,find(p,NETWORK_EDIT_ID));assert(!strcmp(wifi_form.ssid,"TestNetwork"));
        on_click(p,find(p,WIFI_CLEAR_ID));on_click(p,find(p,WIFI_SAVE_ID));assert(wifi_form.active && wifi_form.error && !wifi_commits);
        on_click(p,find(p,WIFI_SHIFT_ID));on_click(p,find(p,WIFI_KEY_ID+10));assert(!strcmp(wifi_form.ssid,"Q"));
        on_timer(p,NETWORK_TIMER);assert(!strcmp(wifi_form.ssid,"Q"));
        on_click(p,find(p,WIFI_PASSWORD_ID));on_click(p,find(p,WIFI_REVEAL_ID));
        assert(strstr(((struct mock_control *)find(p,WIFI_PASSWORD_ID))->text,"testpassword"));
        force_save_failure=1;on_click(p,find(p,WIFI_SAVE_ID));
        assert(wifi_form.active && wifi_form.error && !wifi_commits && !strcmp(config_ssid,"TestNetwork") && retried==before);
        force_save_failure=0;settings.wifi_adb=is("wifi-enabled");settings.adb_force=settings.wifi_adb;
        on_click(p,find(p,WIFI_SAVE_ID));
        assert(!wifi_form.active && wifi_commits==1 && !strcmp(config_ssid,"Q"));
        assert(retried==before+settings.wifi_adb && forced_debug==settings.wifi_adb);
        assert(!*wifi_form.password && !*((struct mock_control *)find(p,WIFI_PASSWORD_ID))->text);
        assert(!strcmp(((struct mock_control *)find(p,NETWORK_NAME_ID))->text,"Q"));
        destroy(p);onEasyUIDeinit((void *)123);puts("PASS: Wi-Fi editor and build version");return 0;
    }
    if(is("external-app") || is("external-empty")) {
        assert(external_app && !ready && !registered && !applied);
        assert(is("external-empty") ? !selected && restarts==1 : selected && !strcmp(selected,"installedActivity"));
        onEasyUIDeinit((void *)123);assert(deinit_calls==1);
        printf("PASS: %s\n",scenario);return 0;
    }
    if(is("recovery-retry") || is("recovery-bounded")) {
        void *p=registered();extension_recovery_ok=0;
        stock_only=is("recovery-bounded");
        launch(p);
        assert(!opened && !extension_calls);
        if(stock_only && is("recovery-bounded"))assert(phase==1 && launch_error && !restarts);
        else {assert(phase==2 && restart_pending);on_timer(p,CONTINUE_TIMER);assert(restarts==1);}
        ((void (*)(void *))(*(void ***)p)[1])(p);
        onEasyUIDeinit((void *)123);
        printf("PASS: %s\n",scenario);return 0;
    }
    if(!strncmp(scenario,"force-",6)) {
        void *p=registered();assert(debug_is_forced() && retried==1);
        if(is("force-cleanup-fail"))settings.adb_force=0;
        on_timer(p,BOOT_TIMER);
        if(is("force-cleanup-fail")) {
            assert(phase==1 && launch_error && !opened);
        } else if(is("force-watchdog-fail")) {
            assert(phase==1 && watchdog_pending && !opened);
            for(int i=0;i<6;++i)on_timer(p,NETWORK_TIMER);
            assert(!watchdog_pending && !opened && launch_error && !watch_started);
        } else {
            assert(phase==3 && opened && !released);
            for(int i=0;i<120;++i)on_timer(p,NETWORK_TIMER);
            assert(adb_polls==120 && watch_beats>=120);
            if(is("force-watchdog"))assert(watch_started);
        }
        destroy(p);onEasyUIDeinit((void *)123);
        assert(!debug_is_forced() && !watch_started && allocations==frees);
        printf("PASS: %s\n",scenario);return 0;
    }
    if(!strncmp(scenario,"autolaunch",10) || is("config-invalid")) {
        assert(!strcmp(selected,ACTIVITY));void *p=registered();
        if(is("config-invalid")){assert(phase==1 && launch_error && !settings.wifi_adb);}
        else if(is("autolaunch-return")){assert(phase==1 && section==2);on_timer(p,BOOT_TIMER);assert(!restarts);}
        else if(is("autolaunch-tap")) {
            on_click(p,find(p,TAP_ID));on_timer(p,BOOT_TIMER);assert(phase==1 && !restarts && !browser_prepared);
            on_click(p,find(p,BROWSER_TAB_ID));on_click(p,find(p,AUTOLAUNCH_ID));assert(!settings.loader_autolaunch && saved==1);
            on_click(p,find(p,AUTOLAUNCH_ID));assert(settings.loader_autolaunch && saved==2);
            force_save_failure=1;on_click(p,find(p,AUTOLAUNCH_ID));assert(settings.loader_autolaunch && !settings_ok);
        } else {
            on_timer(p,BOOT_TIMER);assert(browser_prepared==1 && !opened && !applied);
            if(is("autolaunch-invalid"))assert(phase==1 && !restarts && launch_error);
            else if(is("autolaunch-restart-failure"))assert(phase==1 && restarts==1 && browser_cancelled==1);
            else assert(phase==2 && restarts==1);
        }
        destroy(p);onEasyUIDeinit((void *)123);assert(allocations==frees);
        printf("PASS: %s\n",scenario);return 0;
    }
    if(is("device")) {
        assert(!strcmp(selected,ACTIVITY));void *p=registered();
        on_click(p,find(p,TAP_ID));assert(!device_reads);
        on_click(p,find(p,DEVICE_TAB_ID));assert(section==3 && device_reads==1);
        assert(((struct mock_control *)find(p,DEVICE_PANEL_ID))->shown);
        assert(!((struct mock_control *)find(p,ADB_PANEL_ID))->shown);
        assert(info_count(NULL,NULL)==1);
        info_data(NULL,NULL,find(p,24020),0);
        assert(!strcmp(((struct mock_control *)find(p,24021))->text,"1.2.3"));
        info_data(NULL,NULL,find(p,24020),-1);
        on_timer(p,NETWORK_TIMER);assert(device_reads==1);
        on_click(p,find(p,DEVICE_REFRESH_ID));assert(device_reads==2);
        on_click(p,find(p,ADB_TAB_ID));assert(section==1);
        on_click(p,find(p,DEVICE_REFRESH_ID));assert(device_reads==2);
        assert(!applied && !opened && !saved);
        destroy(p);onEasyUIDeinit((void *)123);assert(allocations==frees);
        printf("PASS: %s\n",scenario);return 0;
    }
    if(is("browser") || is("browser-prepare-failure") || is("browser-restart-failure") || is("external-return") || is("external-invalid") || is("external-load-failure") || is("external-symbol-failure") || is("external-self")) {
        assert(!strcmp(selected,ACTIVITY));void *p=registered();
        if(phase==0) on_click(p,find(p,TAP_ID));
        on_click(p,find(p,BROWSER_TAB_ID));assert(section==2 && browser_opens==1);
        on_click(p,find(p,BROWSER_LOAD_ID));assert(browser_prepared==1 && !applied && !opened);
        if(is("browser-prepare-failure")) assert(phase==1 && !restarts && !released);
        else if(is("browser-restart-failure")) assert(phase==1 && restarts==1 && browser_cancelled==1);
        else assert(phase==2 && restarts==1 && released==1);
        destroy(p);onEasyUIDeinit((void *)123);
        assert(allocations==frees);printf("PASS: %s\n",scenario);return 0;
    }

    if (is("core-failure") || is("rejected-extension") || is("extension") || is("extension-failure") || is("extension-cleanup-failure") || is("config-on") || is("config-off") || is("list") || is("developer-auto") || is("developer-tap") || is("normal") || is("tapped") || is("toggles") || is("patch-failure") || is("save-failure") || is("shutdown-prepared")) {
        assert(!strcmp(selected, ACTIVITY) && registered && create_calls == 1);
        int marked = is("config-on") || is("config-off");
        assert(phase == marked && timer_calls == (marked ? 1 : 3) && !opened);
        assert(retried == (is("config-on") ? 1 : 0));
        if (!is("shutdown-prepared")) {
            void *p = registered();
            assert(!prepared && allocations == 1);
            void **v = *(void ***)p;
            void (*click)(void *, void *) = (void (*)(void *, void *))v[2];
            int (*tick)(void *, int) = (int (*)(void *, int))v[5];
            for (int i = 0; i < 17; ++i) {
                unsigned int previous=loading_dot;
                tick(p, LOADING_TIMER);
                if(!marked) {
                    assert(!((struct mock_control *)find(p,LOADING_DOT_ID+previous))->shown);
                    assert(((struct mock_control *)find(p,LOADING_DOT_ID+loading_dot))->shown);
                }
            }
            assert(loading_dot == (marked ? 0u : 2u) && !opened && !applied);
            for (int i = 0; i < 120; ++i) tick(p, NETWORK_TIMER);
            assert(retried == (is("config-on") ? 1 : 0));
            assert(adb_polls == ((marked || !settings.wifi_adb) ? 120 : 0));
            adb_polls = 0;
            if (marked || is("list") || is("developer-tap") || is("tapped") || is("toggles")) {
                if (!marked) click(p, find(p, TAP_ID));
                assert(phase == 1 && cancelled == (marked ? 0 : 2));
                if (is("developer-tap")) {
                    assert(retried == 1);
                    tick(p, NETWORK_TIMER);
                    assert(adb_polls == 1);
                }
                unsigned int stopped_dot=loading_dot;
                tick(p, LOADING_TIMER);assert(loading_dot==stopped_dot);
                tick(p, BOOT_TIMER); assert(!opened && !applied);
                if (is("toggles")) {
                    click(p, find(p, ADB_TAB_ID)); assert(section == 1);
                    force_save_failure = 1;
                    click(p, find(p, WIFI_ID)); assert(!settings.wifi_adb && !retried && !settings_ok);
                    force_save_failure = 0;
                    click(p, find(p, WIFI_ID)); assert(settings.wifi_adb && retried == 1);
                    ip = "192.0.2.42"; tick(p, NETWORK_TIMER);
                    assert(!strcmp(address, "192.0.2.42:5555") && adb_polls == 1);
                    assert(!strcmp(((struct mock_control *)find(p, NETWORK_NAME_ID))->text, "TestNetwork"));
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
                    obtain(p, 0); assert(!strcmp(((struct mock_control *)find(p, 24000))->text, "Test label"));
                    int before_refresh = list_refreshes;
                    tick(p, NETWORK_TIMER); assert(list_refreshes == before_refresh);
                    item_click(p, 63); assert(!settings.patches[63].enabled && saved == 2);
                }
                click(p, find(p, LAUNCH_ID));
            } else tick(p, BOOT_TIMER);
            if(is("save-failure"))assert(phase==1 && !opened);
            else if(is("core-failure") || is("rejected-extension") || is("patch-failure") || is("extension-failure") || is("extension-cleanup-failure")) {
                assert(phase==2 && !opened);
                assert(strstr(((struct mock_control *)find(p,NOTICE_ID))->text,"Some extensions failed"));
                if(is("patch-failure"))assert(!settings.patches[0].enabled && applied==2);
                else if(!is("core-failure") && !is("rejected-extension"))assert(!settings.patches[1].enabled);
                if(is("core-failure"))assert(stock_only && restored==1 && extension_calls==1);
                tick(p,CONTINUE_TIMER);
                if(is("extension-cleanup-failure"))assert(restarts==1 && !opened);
                else assert(opened==1 && phase==3);
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

void update_poll(void){}
int update_start(const char *action,const char *id){(void)id;updater_action=action;++updater_calls;return 1;}
int update_busy(void){return updater_active;}
const char *update_status(void){return "Select a release";}
unsigned int update_count(void){return is("updates") ? 1:0;}
const struct update_release *update_release(unsigned int index){return is("updates") && index==0 ? &test_release:NULL;}
void update_confirm(void){}
void debug_connect_wifi(void){++wifi_connections;}
