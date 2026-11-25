/* Original C adapter for the inspected ARM FlyThings Activity ABI. */
#include "debug.h"
#include "watchdog.h"
#include "browser.h"
#include "device.h"
#include "extensions/engine.h"
#include "state.h"
#include "wifi_editor.h"
#ifndef DASHBORNE_VERSION
#define DASHBORNE_VERSION "development"
#endif
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
EXPORT void onEasyUIInit(void *);
EXPORT void onEasyUIDeinit(void *);
EXPORT const char *onStartupApp(void *);
#define ACTIVITY "dashborneLoaderActivity"
#define LAYOUT "/mnt/extsd/Dashborne/ui/loader.ftu"
#define BOOT_TIMER 92001
#define NETWORK_TIMER 92002
#define LOADING_TIMER 92003
#define CONTINUE_TIMER 92004
#define STOCK_ONCE "/mnt/extsd/Dashborne/stock.once"
#define LOADING_DOT_ID 50060
#define SPLASH_ID 110001
#define CONFIG_ID 110002
#define TAP_ID 20001
#define WIFI_ID 20002
#define FORCE_ID 20015
#define RESTART_ID 20016
#define LAUNCH_ID 20003
#define PATCHES_TAB_ID 20006
#define ADB_TAB_ID 20007
#define BROWSER_TAB_ID 20008
#define AUTOLAUNCH_ID 20014
#define DEVICE_TAB_ID 20012
#define DEVICE_REFRESH_ID 20013
#define DEVICE_PANEL_ID 110006
#define DEVICE_LIST_ID 80003
#define BROWSER_PANEL_ID 110005
#define BROWSER_LIST_ID 80002
#define BROWSER_UP_ID 20009
#define BROWSER_REFRESH_ID 20010
#define BROWSER_LOAD_ID 20011
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
#define NETWORK_NAME_ID 50027
#define NETWORK_EDIT_ID 20017
#define WIFI_EDITOR_ID 110007
#define WIFI_SSID_ID 20100
#define WIFI_PASSWORD_ID 20101
#define WIFI_SAVE_ID 20102
#define WIFI_CANCEL_ID 20103
#define WIFI_SHIFT_ID 20104
#define WIFI_SYMBOLS_ID 20105
#define WIFI_SPACE_ID 20106
#define WIFI_BACKSPACE_ID 20107
#define WIFI_CLEAR_ID 20108
#define WIFI_REVEAL_ID 20109
#define WIFI_KEY_ID 20200
static struct wifi_editor wifi_form;
static char saved_network[33];

static void *stock, *runtime, *external_app;
static void (*external_deinit)(void *);
static const char *(*external_start)(void *);
static int browser_initialized, browser_returning, browser_failed;
static int (*restart_property)(const char *, const char *);
static int restart_ui(void) {
    if (!restart_property) {
        void *lib = dlopen("/lib/libeasyui.so", 1);
        if (lib) *(void **)(&restart_property) = dlsym(lib, "_ZN16SystemProperties9setStringEPKcS1_");
    }
    return restart_property && restart_property("ctl.restart", "zkswe") == 0;
}
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
static unsigned int watchdog_pending;
static int stock_only,restart_pending;
static unsigned int loading_dot;
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
static void render_wifi(void *self) {
    char text[65];
    wifi_editor_display(&wifi_form,0,text);label(self,WIFI_SSID_ID,text);
    wifi_editor_display(&wifi_form,1,text);label(self,WIFI_PASSWORD_ID,text);
    /* setText copies the input. Clear the temporary, including revealed passwords. */
    volatile char *secret=text;for(unsigned int i=0;i<sizeof(text);++i)secret[i]=0;
    color(self,WIFI_SSID_ID,!wifi_form.field ? 0xffffff : 0xa4abb3);
    color(self,WIFI_PASSWORD_ID,wifi_form.field ? 0xffffff : 0xa4abb3);
    label(self,50102,wifi_form.field ? "Typing password" : "Typing network name");
    label(self,50103,wifi_form.error ? wifi_form.error : "");
    label(self,WIFI_REVEAL_ID,wifi_form.reveal ? "Hide" : "Show");
    label(self,WIFI_SYMBOLS_ID,wifi_form.symbols ? "ABC" : "#+=");
    label(self,WIFI_SHIFT_ID,wifi_form.shift ? "SHIFT" : "Shift");
    for(unsigned int i=0;i<40;++i) {
        char key[2]={wifi_editor_key(&wifi_form,i),0};
        visible(self,WIFI_KEY_ID+(int)i,key[0]!=0);label(self,WIFI_KEY_ID+(int)i,key);
    }
}
static void close_wifi(void *self) {
    label(self,WIFI_PASSWORD_ID,"");label(self,WIFI_SSID_ID,"");
    wifi_editor_close(&wifi_form);
}
static void render(void *self) {
    visible(self, SPLASH_ID, phase == 0);
    visible(self,WIFI_EDITOR_ID,phase==1 && wifi_form.active);
    visible(self, CONFIG_ID, (phase == 1 || phase == 2) && !wifi_form.active);
    if(phase==1 && wifi_form.active){render_wifi(self);return;}
    if (phase != 1 && phase != 2) return;
    label(self,50014,"v" DASHBORNE_VERSION "  /  https://github.com/oliexe/Dashborne");
    visible(self, PATCHES_PANEL_ID, section == 0);
    visible(self, ADB_PANEL_ID, section == 1);
    visible(self, DEVICE_PANEL_ID, section == 3);
    visible(self, 50050, section == 3);
    void *device_tab = find_control(self, DEVICE_TAB_ID);
    if (device_tab) set_selected(device_tab, section == 3);
    color(self, DEVICE_TAB_ID, section == 3 ? 0xf1f2f4 : 0x858c94);
    visible(self, BROWSER_PANEL_ID, section == 2);
    visible(self, 50035, section == 2);
    visible(self, 50020, section == 0); visible(self, 50021, section == 1);
    void *patch_tab = find_control(self, PATCHES_TAB_ID), *adb_tab = find_control(self, ADB_TAB_ID);
    if (patch_tab) set_selected(patch_tab, section == 0);
    if (adb_tab) set_selected(adb_tab, section == 1);
    void *browser_tab = find_control(self, BROWSER_TAB_ID);
    if (browser_tab) set_selected(browser_tab, section == 2);
    color(self, BROWSER_TAB_ID, section == 2 ? 0xf1f2f4 : 0x858c94);
    if (section == 2) {
        label(self, 50041, browser_path());
        const char *status=browser_status();
        char auto_status[128]="";
        if(settings.loader_autolaunch && !browser_can_load()) {
            const char *name=settings.loader_app;for(const char *p=name;*p;++p)if(*p=='/')name=p+1;
            unsigned int at=append(auto_status,0,"Autolaunch: ",sizeof(auto_status));append(auto_status,at,name,sizeof(auto_status));status=auto_status;
        }
        label(self,50042,status);
        label(self,50044,settings.loader_autolaunch ? "1" : "0");
        color(self,AUTOLAUNCH_ID,settings.loader_autolaunch || browser_can_load() ? 0xd9dde2 : 0x666e77);
        visible(self, BROWSER_LOAD_ID, browser_can_load());
    }
    color(self, PATCHES_TAB_ID, section == 0 ? 0xf1f2f4 : 0x858c94);
    color(self, ADB_TAB_ID, section == 1 ? 0xf1f2f4 : 0x858c94);
    label(self, WIFI_ID, "Enable ADB");
    label(self, WIFI_TOGGLE_ID, settings.wifi_adb ? "1" : "0");
    label(self,50066,settings.wifi_adb ? (debug_is_forced() ? "1" : "0") : "2");
    label(self,50067,settings.wifi_adb && debug_is_forced() ? (settings.adb_auto_restart ? "1" : "0") : "2");
    color(self,FORCE_ID,settings.wifi_adb ? 0xd9dde2 : 0x666e77);
    color(self,RESTART_ID,settings.wifi_adb && debug_is_forced() ? 0xd9dde2 : 0x666e77);
    color(self, WIFI_ID, settings.wifi_adb ? 0xc2c9d1 : 0xd9dde2);
    const char *ip = debug_ip();
    label(self, STATUS_ID, debug_status());
    char address[40] = "";
    if (settings.wifi_adb && *ip) {
        unsigned int n = append(address, 0, ip, sizeof(address));
        append(address, n, ":5555", sizeof(address));
    }
    label(self, ADDRESS_ID, *address ? address : "-");
    label(self, NETWORK_NAME_ID, *saved_network ? saved_network : "Set up Wi-Fi");
    unsigned int enabled = 0;
    for (unsigned int i = 0; i < settings.count; ++i) enabled += !!settings.patches[i].enabled;
    char counts[64] = "";
    unsigned int n = number(counts, 0, enabled);
    n = append(counts, n, " enabled / ", sizeof(counts));
    n = number(counts, n, settings.count);
    append(counts, n, settings.count > 5 ? " extensions  -  Swipe to scroll" : " extensions", sizeof(counts));
    label(self, COUNT_ID, counts);
    visible(self, EMPTY_ID, settings.count == 0);
    visible(self, LIST_ID, settings.count != 0);
    void *notice=find_control(self, NOTICE_ID);
    if(notice)set_color(notice,0xff6369);
    label(self, NOTICE_ID, launch_error ? launch_error : settings_ok ? "" : "Could not read or save dashborne.conf");
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
    set_color(item, p->compatible ? 0xe4e7eb : 0x666e77);
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

static int info_count(void *self, void *list) { (void)self; (void)list; return (int)device_count(); }
static void info_data(void *self, void *list, void *item, int index) {
    (void)self; (void)list;
    const struct device_row *row=device_row((unsigned int)index); if(!row)return;
    set_text(item,row->label);
    void *value=find_subitem(item,24021); if(value)set_text(value,row->value);
}
static void *info_adapter_table[]={0,0,(void *)list_noop,(void *)list_noop,(void *)info_count,(void *)info_data};
static void **info_adapter=info_adapter_table+2;
static void refresh_device(void *self) {
    device_refresh();void *list=find_control(self,DEVICE_LIST_ID);if(list)refresh_list(list);
}

static int file_count(void *self, void *list) { (void)self; (void)list; return (int)browser_count(); }
static void file_data(void *self, void *list, void *item, int index) {
    (void)self; (void)list;
    const struct browser_entry *e = browser_entry((unsigned int)index);
    if (!e) return;
    set_text(item, e->name);
    void *kind = find_subitem(item, 24011);
    if (kind) set_text(kind, e->directory ? "Folder  >" : "File");
    const char *selected = browser_selection(), *name = selected;
    for (const char *p=selected; *p; ++p) if (*p=='/') name=p+1;
    const char *a=name,*b=e->name;
    while (*a && *a==*b) { ++a; ++b; }
    set_selected(item, *selected && !*a && !*b);
}
static void file_click(void *self, void *list, int index, int subitem) {
    (void)self; (void)subitem;
    if (phase != 1 || section != 2 || !list_owner || index < 0) return;
    launch_error=(const char *)0;browser_select((unsigned int)index); refresh_list(list); render(list_owner);
}
static void *file_adapter_table[] = {0,0,(void *)list_noop,(void *)list_noop,(void *)file_count,(void *)file_data};
static void *file_listener_table[] = {0,0,(void *)list_noop,(void *)list_noop,(void *)file_click};
static void **file_adapter=file_adapter_table+2, **file_listener=file_listener_table+2;
static void refresh_browser(void *self) {
    void *list=find_control(self,BROWSER_LIST_ID); if(list) refresh_list(list);
}
static void load_app(void *self) {
    if (!browser_prepare()) { render(self); return; }
    phase=2; unregister_timer(self,NETWORK_TIMER); debug_release();
    label(self,50042,"Loading app...");
    if (restart_ui()) return;
    browser_cancel_request(); phase=1;
    launch_error="Could not restart the UI. App was not selected.";
    debug_set_enabled(settings.wifi_adb); register_timer(self,NETWORK_TIMER,1000); render(self);
}

static void toggle_autolaunch(void *self) {
    if(!settings.loader_autolaunch && !browser_can_load())return;
    int before=settings.loader_autolaunch;char previous[BROWSER_PATH];append(previous,0,settings.loader_app,sizeof(previous));
    settings.loader_autolaunch=!before;
    if(settings.loader_autolaunch)append(settings.loader_app,0,browser_selection(),sizeof(settings.loader_app));
    settings_ok=state_save(&settings);
    if(!settings_ok){settings.loader_autolaunch=before;append(settings.loader_app,0,previous,sizeof(settings.loader_app));}
    render(self);
}
static void finish_boot(void *self) {
    phase=3;
    if(!debug_is_forced()){unregister_timer(self,NETWORK_TIMER);debug_release();}
    watchdog_beat();log_line("official-selected\n");
    open_activity(ui_instance(),stock_only?stock_screen:extensions_activity(stock_screen),(void *)0);
}
static int disable_failed(const char *id,int native_only) {
    if(!id || !*id)return 0;
    for(unsigned int i=0;i<settings.count;i++) {
        struct patch_entry *p=&settings.patches[i];
        const char *a=p->id,*b=id;while(*a && *a==*b){a++;b++;}
        if(!*a && !*b && p->enabled && p->native==native_only) {
            p->enabled=0;
            log_line("extensions: disabled ");log_line(id);log_line("\n");
            if(!state_save(&settings))log_line("extensions: could not persist disabled selection\n");
            return 1;
        }
    }
    return 0;
}
static void warn_boot(void *self,int restart) {
    phase=2;restart_pending=restart;
    visible(self,SPLASH_ID,0);visible(self,CONFIG_ID,1);
    label(self,NOTICE_ID,"Some extensions failed to load. Continuing without them...");
    void *notice=find_control(self,NOTICE_ID);if(notice)set_color(notice,0xffbc67);
    register_timer(self,CONTINUE_TIMER,2000);
}
static int prepare_stock_restart(void) {
    int fd=open(STOCK_ONCE,1|0100|01000,0600);
    if(fd<0)return 0;
    int ok=write(fd,"1\n",2)==2 && fsync(fd)==0;
    if(close(fd))ok=0;
    return ok;
}
static void launch(void *self) {
    if (phase == 2 || phase == 3) return;
    if (!extension_recovery_ok) {
        if(stock_only) {
            launch_error="Extension cleanup still failed. Restart without the card to recover.";
            phase=1;render(self);return;
        }
        stock_only=1;
        if(prepare_stock_restart()){warn_boot(self,1);return;}
        launch_error="Could not prepare recovery on the microSD. Restart without the card.";
        phase=1;render(self);return;
    }
    launch_error = (const char *)0;
    if (!state_save(&settings)) { settings_ok = 0; phase = 1; render(self); return; }
    if(debug_is_forced() && !(settings.wifi_adb && settings.adb_force)) {
        launch_error="Force ADB cleanup failed. Restart before continuing.";
        phase=1;render(self);return;
    }
    if(settings.wifi_adb && settings.adb_force && !debug_is_forced()) {
        settings.adb_force=0;settings.adb_auto_restart=0;
        (void)state_save(&settings);
        log_line("debug: Force ADB unavailable; continuing with stock networking\n");
    }
    if(settings.wifi_adb && debug_is_forced() && settings.adb_auto_restart) {
        unregister_timer(self,BOOT_TIMER);unregister_timer(self,LOADING_TIMER);
        if(!watchdog_start()) {
            launch_error="Could not start crash recovery. Check the microSD files.";
            phase=1;render(self);return;
        }
        if(!watchdog_ready()) {
            watchdog_pending=1;phase=1;launch_error="Starting crash recovery...";render(self);return;
        }
    }
    phase = 2; unregister_timer(self, BOOT_TIMER); unregister_timer(self, LOADING_TIMER);
    void *notice=find_control(self, NOTICE_ID);
    if(notice)set_color(notice,0x858c94);
    label(self, NOTICE_ID, "Loading selected extensions...");
    int warned=stock_only || state_rejected_count()!=0;
    if(stock_only){state_restore();warn_boot(self,0);return;}
    int result;
    for(unsigned int attempt=0;;attempt++) {
        result=state_apply(&settings,log_line);
        if(result>=0)break;
        warned=1;state_restore();
        if(attempt>=MAX_PATCHES || !disable_failed(state_failed_id(),0)) {
            /* File/mount failure: do not depend on a partially composed layout. */
            stock_only=1;warn_boot(self,0);return;
        }
    }
    for(unsigned int attempt=0;;attempt++) {
        const char *extensions[MAX_PATCHES];unsigned int count=0;
        for(unsigned int i=0;i<settings.count;i++)
            if(settings.patches[i].enabled && settings.patches[i].native)extensions[count++]=settings.patches[i].id;
        int activated=extensions_start(extensions,count,stock,runtime,
            result>0?"/res/ui/dash1act.ftu":(const char *)0,log_line);
        if(activated>=0)break;
        warned=1;
        int disabled=disable_failed(extensions_failed_id(),1);
        if(activated==-2) {
            /* Never launch native UI in a process with unretired callbacks. */
            extension_recovery_ok=0;state_restore();stock_only=1;
            if(prepare_stock_restart()){warn_boot(self,1);return;}
            launch_error="Could not prepare recovery on the microSD. Restart without the card.";
            phase=1;render(self);return;
        }
        if(!disabled || attempt>=MAX_PATCHES) {
            state_restore();stock_only=1;
            log_line("core: unavailable; continuing with stock dashboard\n");break;
        }
    }
    if(warned){warn_boot(self,0);return;}
    finish_boot(self);
}
static void continue_boot(void *self) {
    if(!settings.loader_autolaunch){launch(self);return;}
    unregister_timer(self,BOOT_TIMER);unregister_timer(self,LOADING_TIMER);
    if(!browser_prepare_path(settings.loader_app)) {
        phase=1;section=2;launch_error=browser_status();
        if(!browser_initialized){browser_init();browser_initialized=1;refresh_browser(self);}
        debug_set_enabled(settings.wifi_adb);render(self);return;
    }
    /* The existing one-use request/return marker prevents a crashing app from
       being selected again on the service's automatic restart. */
    phase=2;unregister_timer(self,NETWORK_TIMER);debug_release();
    if(restart_ui())return;
    browser_cancel_request();phase=1;launch_error="Could not restart the UI for autolaunch.";
    debug_set_enabled(settings.wifi_adb);register_timer(self,NETWORK_TIMER,1000);render(self);
}
static int on_timer(void *self, int id) {
    if(id==CONTINUE_TIMER && phase==2) {
        if(restart_pending) {
            watchdog_stop();
            if(!restart_ui()) {
                phase=1;launch_error="Could not restart the UI. Restart the DASH to recover.";render(self);
            }
        } else finish_boot(self);
        return 0;
    }
    if (id == LOADING_TIMER) {
        if (phase == 0) {
            visible(self, LOADING_DOT_ID + loading_dot, 0);
            loading_dot = (loading_dot + 1) % 3;
            visible(self, LOADING_DOT_ID + loading_dot, 1);
        }
        return 1;
    }
    if (id == BOOT_TIMER && phase == 0) { continue_boot(self); return 1; }
    if(id==NETWORK_TIMER)watchdog_beat();
    if(id==NETWORK_TIMER && watchdog_pending) {
        if(watchdog_ready()) {watchdog_pending=0;launch(self);}
        else if(++watchdog_pending>=6) {
            watchdog_pending=0;watchdog_stop();
            launch_error="Crash recovery unavailable or restart limit reached. Reboot to reset.";
        }
    }
    if (id == NETWORK_TIMER && (phase == 1 || debug_is_forced() || (phase == 0 && !settings.wifi_adb))) {
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
    wifi_editor_close(&wifi_form);
    wifi_saved_name(saved_network);
    state_load(&settings);
    settings_ok=settings.config_ok;
    if(access(STOCK_ONCE,0)==0) {
        extern int unlink(const char *);
        stock_only=1;(void)unlink(STOCK_ONCE);
    }
    phase = browser_returning ? 1 : 0; section = browser_returning ? 2 : 0;
    launch_error = browser_failed ? "App could not be loaded. See loader.log." : (const char *)0;
    if(!settings.config_ok){phase=1;launch_error="Could not read dashborne.conf. Check the microSD configuration.";}
    list_owner = self;
    void *list = find_control(self, LIST_ID);
    if (list) {
        set_adapter(list, &adapter); set_list_listener(list, &list_listener);
        refresh_list(list);
    }
    void *info=find_control(self,DEVICE_LIST_ID);
    if(info)set_adapter(info,&info_adapter);
    void *files=find_control(self,BROWSER_LIST_ID);
    if(files) { set_adapter(files,&file_adapter); set_list_listener(files,&file_listener); }
    if(browser_returning) { browser_init(); browser_initialized=1; refresh_browser(self); }
    /* A one-shot deployment marker opens configuration for remote verification. */
    if (access("/mnt/extsd/Dashborne/config.once", 0) == 0) {
        extern int unlink(const char *);
        (void)unlink("/mnt/extsd/Dashborne/config.once"); phase = 1;
    }
    if(watchdog_recovery()) {
        phase=1;launch_error="UI recovered. Crash logs are in Dashborne/diagnostics.";
    }
    if (phase == 1 || !settings.wifi_adb || settings.adb_force) debug_set_enabled(settings.wifi_adb);
    if(settings.wifi_adb && settings.adb_force)(void)debug_set_forced(1);
    render(self);
    register_timer(self, NETWORK_TIMER, 1000);
    loading_dot = 0;
    if (!phase) {
        register_timer(self, BOOT_TIMER, 3000);
        register_timer(self, LOADING_TIMER, 180);
    }
    log_line(phase ? "configuration-created\n" : "splash-created\n");
}
static void on_click(void *self, void *control) {
    int id = control_id(control);
    if(phase==1 && wifi_form.active) {
        if(id==WIFI_CANCEL_ID)close_wifi(self);
        else if(id==WIFI_SAVE_ID) {
            if(wifi_editor_save(&wifi_form)) {
                wifi_saved_name(saved_network);close_wifi(self);settings_ok=1;
                if(settings.wifi_adb) {
                    debug_set_enabled(1);
                    if(settings.adb_force)(void)debug_set_forced(1);
                }
            }
        } else if(id==WIFI_SSID_ID || id==WIFI_PASSWORD_ID) {
            wifi_form.field=id==WIFI_PASSWORD_ID;wifi_form.error=0;
        } else if(id==WIFI_SHIFT_ID)wifi_form.shift=!wifi_form.shift;
        else if(id==WIFI_SYMBOLS_ID)wifi_form.symbols=!wifi_form.symbols;
        else if(id==WIFI_REVEAL_ID)wifi_form.reveal=!wifi_form.reveal;
        else if(id==WIFI_BACKSPACE_ID)wifi_editor_backspace(&wifi_form);
        else if(id==WIFI_CLEAR_ID)wifi_editor_clear_field(&wifi_form);
        else if(id==WIFI_SPACE_ID)wifi_editor_type(&wifi_form,' ');
        else if(id>=WIFI_KEY_ID && id<WIFI_KEY_ID+40)wifi_editor_type(&wifi_form,wifi_editor_key(&wifi_form,(unsigned int)(id-WIFI_KEY_ID)));
        render(self);return;
    }
    if (id == TAP_ID && phase == 0) {
        phase = 1; unregister_timer(self, BOOT_TIMER); unregister_timer(self, LOADING_TIMER); log_line("configuration-entered\n");
        debug_set_enabled(settings.wifi_adb);
    } else if (phase != 1) return;
    else if(watchdog_pending)return;
    else if (id == LAUNCH_ID) { continue_boot(self); return; }
    else if (id == PATCHES_TAB_ID) section = 0;
    else if (id == ADB_TAB_ID) section = 1;
    else if (id == DEVICE_TAB_ID) { section=3;refresh_device(self); }
    else if (id == DEVICE_REFRESH_ID && section == 3) refresh_device(self);
    else if (id == BROWSER_TAB_ID) {
        section=2; if(!browser_initialized) { browser_init();browser_initialized=1;refresh_browser(self); }
    } else if (id == BROWSER_UP_ID && section == 2) { browser_up();refresh_browser(self); }
    else if (id == BROWSER_REFRESH_ID && section == 2) { browser_refresh();refresh_browser(self); }
    else if (id == AUTOLAUNCH_ID && section == 2) { toggle_autolaunch(self);return; }
    else if (id == BROWSER_LOAD_ID && section == 2) { load_app(self);return; }
    else if((id==NETWORK_EDIT_ID || id==NETWORK_NAME_ID || id==50076) && section==1) {
        if(!wifi_editor_open(&wifi_form))launch_error="Could not read dashborne.conf";
    }
    else if (id == WIFI_ID && section == 1) {
        settings.wifi_adb = !settings.wifi_adb;
        settings_ok = state_save(&settings);
        /* A failed save must never grant developer access. Disable immediately
           even if the card cannot persist that choice; keep the error visible. */
        if (!settings_ok && settings.wifi_adb) settings.wifi_adb = 0;
        debug_set_enabled(settings.wifi_adb);
        if(settings.wifi_adb && settings.adb_force)(void)debug_set_forced(1);
        if(!settings.wifi_adb)watchdog_stop();
    } else if(id==FORCE_ID && section==1 && settings.wifi_adb) {
        int before=settings.adb_force;settings.adb_force=!before;
        settings_ok=state_save(&settings);
        if(!settings_ok && settings.adb_force)settings.adb_force=before;
        (void)debug_set_forced(settings.adb_force);
        if(!settings.adb_force)watchdog_stop();
    } else if(id==RESTART_ID && section==1 && settings.wifi_adb && debug_is_forced()) {
        int before=settings.adb_auto_restart;settings.adb_auto_restart=!before;
        settings_ok=state_save(&settings);
        if(!settings_ok && settings.adb_auto_restart)settings.adb_auto_restart=before;
        if(!settings.adb_auto_restart)watchdog_stop();
    }
    render(self);
}

static void destroy(void *self) {
    wifi_editor_close(&wifi_form);
    unregister_timer(self, LOADING_TIMER);
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
    device_bind(stock, runtime);
    return 1;
}

EXPORT void onEasyUIInit(void *ctx) {
    log_line("startup\n");
    extension_recovery_ok = extensions_recover(log_line);
    char requested[BROWSER_PATH];
    int request=browser_take_request(requested);
    if(request > 0 && extension_recovery_ok) {
        void *app=dlopen(requested,1);
        void (*init)(void *)=(void *)0;
        if(app) {
            *(void **)(&init)=dlsym(app,"onEasyUIInit");
            *(void **)(&external_deinit)=dlsym(app,"onEasyUIDeinit");
            *(void **)(&external_start)=dlsym(app,"onStartupApp");
        }
        if(init && external_deinit && external_start && init != onEasyUIInit &&
           external_deinit != onEasyUIDeinit && external_start != onStartupApp) {
            external_app=app;log_line("sd-app-selected\n");init(ctx);return;
        }
        log_line("sd-app-load-failed\n");
    }
    browser_failed=request!=0;
    browser_returning=browser_take_return() || browser_failed;
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
    if(external_app) {
        const char *name=external_start(ctx);
        if(name && *name) return name;
        log_line("sd-app-startup-empty\n"); (void)restart_ui(); return (const char *)0;
    }
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
    if(external_app) { external_deinit(ctx);return; }
    /* Leave the init-owned daemon alone, but cancel all pending work. */
    debug_release();
    watchdog_stop();
    (void)extensions_stop();
    if (prepared) { destroy(prepared); prepared = (void *)0; }
    log_line("shutdown\n");
    if (stock_deinit) stock_deinit(ctx);
}
