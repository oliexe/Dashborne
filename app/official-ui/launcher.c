/* Forward the official UI unchanged; reconnect development Wi-Fi once. */
#include "../dashboard/wifi_debug.h"
#include "../adb-enabler/tcp_adb.h"
typedef unsigned long thread_id;
extern void *dlopen(const char *, int);
extern void *dlsym(void *, const char *);
extern int pthread_create(thread_id *, const void *, void *(*)(void *), void *);
extern int pthread_join(thread_id, void **);
extern unsigned int sleep(unsigned int);
extern int open(const char *, int, ...);
extern int write(int, const void *, unsigned int);
extern int close(int);
#define EXPORT __attribute__((visibility("default")))
static void (*original_init)(void *), (*original_deinit)(void *);
static const char *(*original_start)(void *);
static thread_id worker;
static int worker_started;
static _Atomic int stopping;
static void log_line(const char *text) {
    unsigned int length = 0;
    while (text[length]) ++length;
    int fd = open("/mnt/extsd/Dashborne/official-preview.log", 1 | 0100 | 02000, 0644);
    if (fd >= 0) { (void)write(fd, text, length); (void)close(fd); }
}
static void *connect_development_wifi(void *unused) {
    (void)unused;
    /* Allow the official activity to finish its own network initialization. */
    for (int i = 0; i < 15 && !stopping; ++i) sleep(1);
    if (stopping) return (void *)0;
    wifi_debug_retry();
    for (int i = 0; i < 95 && !stopping; ++i) {
        sleep(1);
        if (stopping) break;
        wifi_debug_poll();
        tcp_adb_poll(*wifi_debug_ip() != 0);
        const char *status = tcp_adb_status(), *ready = "Wi-Fi ADB ready";
        while (*ready && *ready == *status) { ++ready; ++status; }
        if (*wifi_debug_ip() && !*ready && !*status) {
            log_line("official-ui: development Wi-Fi connected\n");
            return (void *)0;
        }
    }
    log_line("official-ui: Wi-Fi reconnect ended\n");
    return (void *)0;
}
EXPORT void onEasyUIInit(void *context) {
    void *original = dlopen("/res/lib/libzkgui.so", 1);
    if (!original) { log_line("official-ui: library unavailable\n"); return; }
    *(void **)(&original_init) = dlsym(original, "onEasyUIInit");
    *(void **)(&original_deinit) = dlsym(original, "onEasyUIDeinit");
    *(void **)(&original_start) = dlsym(original, "onStartupApp");
    if (original_init) original_init(context);
    wifi_debug_bind(original, log_line);
    void *runtime = dlopen("/lib/libeasyui.so", 1);
    if (runtime) tcp_adb_bind(runtime, log_line);
    log_line("official-ui: initialized\n");
}
EXPORT const char *onStartupApp(void *context) {
    const char *activity = original_start ? original_start(context) : (const char *)0;
    if (activity && *activity && !worker_started) {
        stopping = 0;
        worker_started = pthread_create(&worker, (const void *)0, connect_development_wifi, (void *)0) == 0;
        if (!worker_started) log_line("official-ui: Wi-Fi worker unavailable\n");
    }
    return activity;
}
EXPORT void onEasyUIDeinit(void *context) {
    stopping = 1;
    if (worker_started) { pthread_join(worker, (void *)0); worker_started = 0; }
    if (original_deinit) original_deinit(context);
}
