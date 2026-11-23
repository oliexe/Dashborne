#include <assert.h>
#include <stdio.h>
#include <string.h>
#define dlopen mock_dlopen
#define dlsym mock_dlsym
#define pthread_create mock_create
#define pthread_join mock_join
#define sleep mock_sleep
#define open mock_open
#define write mock_write
#define close mock_close
#include "../launcher.c"
static int inits, starts, deinits, joins, seconds, retries, polls, fail_thread, never_ready;
static void *(*pending)(void *);
static void init(void *context) { assert(context == (void *)123); ++inits; }
static const char *start(void *context) { assert(context == (void *)123); ++starts; return "dash1actActivity"; }
static void deinit(void *context) { assert(context == (void *)123); ++deinits; }
void *mock_dlopen(const char *path, int flags) { assert(flags == 1); if (!strcmp(path,"/lib/libeasyui.so")) return (void *)2; assert(!strcmp(path,"/res/lib/libzkgui.so")); return (void *)1; }
void *mock_dlsym(void *handle, const char *name) {
    assert(handle == (void *)1);
    if (!strcmp(name,"onEasyUIInit")) return (void *)init;
    if (!strcmp(name,"onStartupApp")) return (void *)start;
    assert(!strcmp(name,"onEasyUIDeinit")); return (void *)deinit;
}
int mock_create(thread_id *id, const void *attr, void *(*fn)(void *), void *arg) {
    assert(!attr && !arg); if (fail_thread) return 1; *id=42; pending=fn; return 0;
}
int mock_join(thread_id id, void **result) { assert(id==42 && !result); ++joins; return 0; }
unsigned int mock_sleep(unsigned int n) { seconds+=(int)n; return 0; }
int mock_open(const char *path, int flags, ...) { assert(path && flags); return 1; }
int mock_write(int fd, const void *text, unsigned int n) { assert(fd==1 && text); return (int)n; }
int mock_close(int fd) { assert(fd==1); return 0; }
void wifi_debug_bind(void *stock, void (*logger)(const char *)) { assert(stock==(void *)1 && logger); }
void wifi_debug_retry(void) { assert(seconds==15); ++retries; }
void wifi_debug_poll(void) { ++polls; }
const char *wifi_debug_ip(void) { return polls>=5 ? "192.0.2.1" : ""; }
void tcp_adb_bind(void *runtime, void (*logger)(const char *)) { assert(runtime==(void *)2 && logger); }
void tcp_adb_poll(int network_ready) { assert(network_ready == (polls>=5)); }
const char *tcp_adb_status(void) { return polls>=7 && !never_ready ? "Wi-Fi ADB ready" : "Starting Wi-Fi ADB"; }
int main(void) {
    onEasyUIInit((void *)123);
    assert(!strcmp(onStartupApp((void *)123),"dash1actActivity"));
    assert(inits==1 && starts==1 && worker_started);
    pending(0);
    assert(retries==1 && polls==7 && seconds==22);
    onEasyUIDeinit((void *)123);
    assert(joins==1 && deinits==1 && !worker_started);
    /* A failed debug worker must still select the unmodified official UI. */
    fail_thread=1;
    assert(!strcmp(onStartupApp((void *)123),"dash1actActivity"));
    assert(!worker_started);
    /* Stopping during the delay must not start a network request. */
    stopping=1; pending(0); assert(retries==1);
    stopping=0; never_ready=1; seconds=0; polls=0; retries=0;
    pending(0); assert(retries==1 && polls==95 && seconds==110);
    puts("PASS: official callbacks, delayed Wi-Fi reconnect, worker failure and shutdown");
}
