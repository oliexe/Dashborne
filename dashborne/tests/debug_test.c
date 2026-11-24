/* Fault injection against the controller, with real parsing and state transitions. */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define dlsym mock_dlsym
#define open mock_open
#define read mock_read
#define close mock_close
#define socket mock_socket
#define ioctl mock_ioctl
#define __errno_location mock_errno
#include "../debug.c"

static int config_valid;
static char configured_ssid[33], configured_password[64], proc4[2048], proc6[2048], mock_daemon[128], port[128];
static unsigned int offsets[3], chunk;
static int mode, connected, carrier, association, correct_ssid, ioctl_error, error_number;
static unsigned char ipv4[4];
static int modes, connects, disconnects, starts, stops, writes, opens, closes;
static int stop_delay, pending_stop, start_delay, pending_start, mode_delay, pending_mode;
static int fail_stop, fail_start, fail_port, fail_get, fail_open, fail_read, fail_socket;
static int automatic_link, automatic_listener, missing_api, read_error_after;
static int events, essid_nul;
static const char *valid_line = " 0: 00000000:15B3 00000000:0000 0A 00000000:00000000\n";
static void mode_change(int value) {
    assert(value == 2);
    ++modes;
    ++events;
    pending_mode = mode_delay + 1;
}
static int mode_get(void) { return mode; }
static int link_get(void) { return connected; }
static void wifi_connect(const void *a, const void *b) {
    const struct string_view_abi *s = a, *p = b;
    assert(s->size && s->size <= 32 && p->size >= 8 && p->size <= 63);
    assert(strlen(s->data) == s->size && strlen(p->data) == p->size);
    ++connects;
    ++events;
    if (automatic_link) {
        connected = carrier = association = correct_ssid = 1;
    }
}
static void wifi_disconnect(void) {
    ++disconnects;
    ++events;
    connected = carrier = association = 0;
}
static int property_get(const char *key, char *out, const char *fallback) {
    (void)fallback;
    if (fail_get)
        return -1;
    strcpy(out, !strcmp(key, "init.svc.adbd") ? mock_daemon : port);
    return (int)strlen(out);
}
static int property_set(const char *key, const char *value) {
    ++writes;
    ++events;
    if (!strcmp(key, "ctl.stop")) {
        assert(!strcmp(value, "adbd"));
        ++stops;
        if (fail_stop)
            return -1;
        pending_stop = stop_delay + 1;
    } else if (!strcmp(key, "ctl.start")) {
        assert(!strcmp(value, "adbd"));
        ++starts;
        if (fail_start)
            return -1;
        pending_start = start_delay + 1;
    } else {
        assert(!strcmp(key, "service.adb.tcp.port"));
        if (fail_port)
            return -1;
        strcpy(port, value);
    }
    return 0;
}
void *mock_dlsym(void *handle, const char *name) {
    (void)handle;
    if (missing_api == 1)
        return NULL;
    if (strstr(name, "change_mode"))
        return (void *)mode_change;
    if (strstr(name, "get_mode"))
        return (void *)mode_get;
    if (strstr(name, "is_connected"))
        return (void *)link_get;
    if (strstr(name, "disconnect"))
        return (void *)wifi_disconnect;
    if (strstr(name, "connect"))
        return missing_api == 2 ? NULL : (void *)wifi_connect;
    if (strstr(name, "getString"))
        return (void *)property_get;
    if (strstr(name, "setString"))
        return (void *)property_set;
    assert(0);
    return NULL;
}
int config_wifi(char s[33],char p[64]) {
    s[0]=p[0]=0;if(!config_valid || fail_open || fail_read)return 0;
    strcpy(s,configured_ssid);strcpy(p,configured_password);return 1;
}
int mock_open(const char *path,int flags,...) {
    assert(flags==0);int fd;
    if(!strcmp(path,"/proc/net/tcp"))fd=1;
    else {assert(!strcmp(path,"/proc/net/tcp6"));fd=2;}
    offsets[fd]=0;++opens;return fd;
}
int mock_read(int fd, void *out, unsigned int size) {
    assert(fd >= 0 && fd <= 2);
    if (fail_read || (read_error_after && offsets[fd] >= (unsigned int)read_error_after))
        return -1;
    const char *src = fd == 1 ? proc4 : proc6;
    unsigned int len = (unsigned int)strlen(src);
    if (offsets[fd] >= len)
        return 0;
    if (size > chunk)
        size = chunk;
    if (size > len - offsets[fd])
        size = len - offsets[fd];
    memcpy(out, src + offsets[fd], size);
    offsets[fd] += size;
    return (int)size;
}
int mock_close(int fd) {
    assert(fd >= 0 && fd <= 3);
    ++closes;
    return 0;
}
int mock_socket(int a, int b, int c) {
    assert(a == 2 && b == 2 && !c);
    if (fail_socket)
        return -1;
    ++opens;
    return 3;
}
int *mock_errno(void) { return &error_number; }
int mock_ioctl(int fd, unsigned long request, ...) {
    assert(fd == 3);
    va_list args;
    va_start(args, request);
    void *p = va_arg(args, void *);
    va_end(args);
    assert(!strcmp(p, "wlan0"));
    unsigned char *r = p;
    if (request == 0x8913)
        r[16] = (unsigned char)(carrier ? 0x41 : 1);
    else if (request == 0x8b15 || request == 0x8b1b) {
        if (ioctl_error) {
            error_number = ioctl_error;
            return -1;
        }
        if (request == 0x8b15) {
            memset(r + 18, 0, 6);
            if (association) {
                r[18] = 2;
                r[23] = 1;
            }
        } else {
            struct essid_request *e = p;
            const char *s = correct_ssid ? ssid : "Different";
            memcpy(e->value.point.pointer, s, strlen(s));
            e->value.point.length = (unsigned short)(strlen(s) + essid_nul);
        }
    } else {
        assert(request == 0x8915);
        r[16] = 2;
        r[17] = 0;
        memcpy(r + 20, ipv4, 4);
    }
    return 0;
}
static void tick(void) {
    if (pending_mode && !--pending_mode)
        mode = 2;
    if (pending_stop && !--pending_stop) {
        strcpy(mock_daemon, "stopped");
        proc4[0] = proc6[0] = 0;
        pending_start = 0;
    }
    if (pending_start && !--pending_start) {
        strcpy(mock_daemon, "running");
        if (automatic_listener)
            strcpy(proc4, valid_line);
    }
    debug_poll();
}
static void advance(int n) {
    while (n--)
        tick();
}
static void reset(void) {
    assert(opens == closes);
    debug_release();
    status = "Disabled";
    step = STOP;
    config_valid=1;strcpy(configured_ssid,"TestNetwork");strcpy(configured_password,"password123");
    proc4[0] = proc6[0] = 0;
    strcpy(mock_daemon, "stopped");
    strcpy(port, "-1");
    mode = 1;
    connected = carrier = association = 0;
    correct_ssid = 1;
    ipv4[0] = 10;
    ipv4[1] = 0;
    ipv4[2] = 1;
    ipv4[3] = 35;
    modes = connects = disconnects = starts = stops = writes = events = 0;
    opens = closes = 0;
    chunk = 7;
    stop_delay = pending_stop = start_delay = pending_start = mode_delay = pending_mode = 0;
    fail_stop = fail_start = fail_port = fail_get = fail_open = fail_read = fail_socket = 0;
    ioctl_error = missing_api = read_error_after = essid_nul = 0;
    automatic_link = automatic_listener = 1;
    debug_bind((void *)1, (void *)2, NULL);
}
static void ready(void) {
    debug_set_enabled(1);
    advance(40);
    assert(!strcmp(debug_ip(), "10.0.1.35"));
    assert(strstr(debug_status(), "ADB listening"));
}
static void cancelled(void) {
    debug_release();
    int before = events;
    advance(500);
    assert(events == before && !*debug_ip() && !*debug_wifi_name());
    for (unsigned int i = 0; i < sizeof(password); ++i)
        assert(!password[i]);
    for (unsigned int i = 0; i < sizeof(ssid); ++i)
        assert(!ssid[i]);
}
static void test_default_and_normal(void) {
    reset();
    debug_set_enabled(0);
    advance(200);
    assert(!events && !strcmp(debug_status(), "Disabled"));
    ready();
    assert(modes == 1 && connects == 1 && starts == 1);
    assert(!strcmp(debug_wifi_name(), "TestNetwork"));
    ipv4[3] = 36;
    tick();
    assert(!strcmp(debug_ip(), "10.0.1.36"));
    debug_set_enabled(0);
    assert(!*debug_ip() && !*debug_wifi_name() && strcmp(debug_status(), "Disabled"));
    advance(10);
    assert(!strcmp(debug_status(), "Disabled") && !strcmp(port, "-1"));
    int before = events;
    advance(100);
    assert(events == before);
    reset();
    mode = 2;
    connected = carrier = association = 1;
    ready();
    assert(!modes && !connects && !disconnects);
    cancelled();
}
static void test_config(void) {
    reset();config_valid=0;debug_set_enabled(1);advance(100);
    assert(strstr(debug_status(),"invalid") && !modes && !connects && !starts);
    reset();memset(configured_ssid,'s',32);configured_ssid[32]=0;
    memset(configured_password,'p',63);configured_password[63]=0;essid_nul=1;
    ready();assert(strlen(ssid)==32 && strlen(password)==63);cancelled();
    reset();
    fail_open = 1;
    debug_set_enabled(1);
    assert(strstr(debug_status(), "invalid"));
    reset();
    fail_read = 1;
    debug_set_enabled(1);
    assert(strstr(debug_status(), "invalid"));
    reset();
    missing_api = 1;
    debug_bind(NULL, NULL, NULL);
    debug_set_enabled(1);
    assert(strstr(debug_status(), "unavailable"));
    reset();
    missing_api = 2;
    debug_bind(NULL, NULL, NULL);
    debug_set_enabled(1);
    assert(strstr(debug_status(), "unavailable"));
}
static void test_service(void) {
    reset();
    ready();
    stop_delay = 3;
    debug_set_enabled(0);
    debug_set_enabled(1);
    assert(!*debug_ip());
    advance(40);
    assert(*debug_ip());
    debug_set_enabled(0);
    debug_set_enabled(1);
    debug_set_enabled(0);
    advance(40);
    assert(!*debug_ip() && !strcmp(debug_status(), "Disabled"));
    reset();
    ready();
    fail_stop = 1;
    debug_set_enabled(0);
    advance(100);
    assert(strstr(debug_status(), "did not stop") && stops == 3 && !*debug_ip());
    fail_stop = 0;
    debug_set_enabled(0);
    advance(10);
    assert(!strcmp(debug_status(), "Disabled"));
    reset();
    strcpy(port, "5555");
    fail_port = 1;
    debug_set_enabled(0);
    advance(100);
    assert(strstr(debug_status(), "could not be cleared") && writes == 3);
    reset();
    fail_get = 1;
    debug_set_enabled(0);
    assert(strstr(debug_status(), "Cannot read"));
    reset();
    fail_start = 1;
    debug_set_enabled(1);
    advance(200);
    assert(strstr(debug_status(), "listener failed") && starts == 3 && !*debug_ip());
    fail_start = 0;
    ready();
    cancelled();
    reset();
    fail_port = 1;
    debug_set_enabled(1);
    advance(200);
    assert(strstr(debug_status(), "listener failed") && starts == 0 && !*debug_ip());
    reset();
    automatic_listener = 0;
    debug_set_enabled(1);
    advance(200);
    assert(strstr(debug_status(), "listener failed") && stops > 0 && stops <= 3 && !*debug_ip());
    reset();
    strcpy(mock_daemon, "running");
    strcpy(port, "-1");
    debug_set_enabled(1);
    advance(100);
    assert(strstr(debug_status(), "another transport") && !events);
    debug_set_enabled(0);
    assert(!events && strstr(debug_status(), "another ADB transport"));
    reset();
    ready();
    strcpy(port, "1234");
    int before = events;
    tick();
    assert(strstr(debug_status(), "another transport") && !*debug_ip() && events == before);
    reset();
    start_delay = 12;
    ready();
    assert(starts == 1);
    cancelled();
    reset();
    start_delay = 20;
    debug_set_enabled(1);
    while (!starts)
        tick();
    assert(!strcmp(mock_daemon, "stopped") && pending_start);
    debug_set_enabled(0);
    advance(50);
    assert(!strcmp(mock_daemon, "stopped") && !strcmp(debug_status(), "Disabled") &&
           !pending_start && stops == 1);
    cancelled();
}
static void test_network(void) {
    for (int kind = 0; kind < 7; ++kind) {
        reset();
        ready();
        automatic_link = 0;
        if (kind == 0)
            connected = 0;
        if (kind == 1)
            carrier = 0;
        if (kind == 2)
            association = 0;
        if (kind == 3)
            correct_ssid = 0;
        if (kind == 4)
            mode = 1;
        if (kind == 5)
            fail_socket = 1;
        if (kind == 6)
            ioctl_error = 5;
        tick();
        assert(!*debug_ip());
        advance(400);
        assert(strstr(debug_status(), "Wi-Fi connection failed") && connects <= 4);
        cancelled();
    }
    const unsigned char bad[][4] = {
        {0, 0, 0, 0}, {127, 0, 0, 1}, {169, 254, 4, 5}, {224, 0, 0, 1}, {255, 255, 255, 255}};
    for (unsigned int i = 0; i < sizeof(bad) / sizeof(*bad); ++i) {
        reset();
        ready();
        memcpy(ipv4, bad[i], 4);
        tick();
        assert(!*debug_ip());
        cancelled();
    }
    reset();
    ioctl_error = 95;
    ready();
    assert(strstr(debug_status(), "limited"));
    cancelled();
    reset();
    ioctl_error = 25;
    ready();
    assert(strstr(debug_status(), "limited"));
    cancelled();
    reset();
    mode_delay = 1000;
    debug_set_enabled(1);
    advance(100);
    assert(strstr(debug_status(), "Wi-Fi connection failed") && modes == 3 && !connects);
    cancelled();
    reset();
    automatic_link = 0;
    debug_set_enabled(1);
    advance(300);
    assert(connects == 3 && strstr(debug_status(), "failed"));
    automatic_link = 1;
    ready();
    cancelled();
}
static void test_parser(void) {
    reset();
    assert(listener_line(valid_line, "10.0.1.35"));
    assert(listener_line("0: 2301000A:15B3 00000000:0000 0A", "10.0.1.35"));
    assert(!listener_line("0: 2301000A:15B3 00000000:0000 0A", "10.0.1.36"));
    assert(!listener_line("0: 0100007F:15B3 00000000:0000 0A", "10.0.1.35"));
    assert(!listener_line("0: 00000000:15B3 00000000:0000 01", "10.0.1.35"));
    assert(!listener_line("0: 00000000:15B4 00000000:0000 0A", "10.0.1.35"));
    assert(listener_line("0: 00000000000000000000000000000000:15B3 00000000:0000 0A", "10.0.1.35"));
    for (unsigned int i = 0; i < strlen(valid_line); ++i) {
        char line[100];
        memcpy(line, valid_line, i);
        line[i] = 0;
        listener_line(line, "10.0.1.35");
    }
    memset(proc4, 'x', 700);
    proc4[700] = '\n';
    strcpy(proc4 + 701, valid_line);
    assert(listener_file("/proc/net/tcp", "10.0.1.35"));
    strcpy(proc4, valid_line);
    proc4[strlen(proc4) - 1] = 0;
    chunk = 1;
    assert(listener_file("/proc/net/tcp", "10.0.1.35"));
    read_error_after = (int)strlen(proc4);
    assert(!listener_file("/proc/net/tcp", "10.0.1.35"));
    reset();
    ready();
    proc4[0] = 0;
    strcpy(proc6, "0: 00000000000000000000000000000000:15B3 00000000:0000 0A\n");
    tick();
    assert(*debug_ip());
    cancelled();
}
static void test_cancellation_and_stress(void) {
    for (int at = 0; at < 45; ++at) {
        reset();
        debug_set_enabled(1);
        advance(at);
        cancelled();
        reset();
        debug_set_enabled(1);
        advance(at);
        debug_set_enabled(0);
        advance(30);
        int before = events;
        advance(100);
        assert(!*debug_ip() && events == before && !strcmp(debug_status(), "Disabled"));
    }
    reset();
    unsigned int seed = 12345;
    for (int i = 0; i < 10000; ++i) {
        seed = seed * 1664525u + 1013904223u;
        switch ((seed >> 16) % 16) {
        case 0:
            debug_set_enabled(0);
            break;
        case 1:
            debug_set_enabled(1);
            break;
        case 2:
            connected = 0;
            break;
        case 3:
            fail_start = !fail_start;
            break;
        case 4:
            fail_stop = !fail_stop;
            break;
        case 5:
            fail_get = !fail_get;
            break;
        case 6:
            mode = 1;
            break;
        case 7:
            proc4[0] = 0;
            break;
        default:
            tick();
            break;
        }
        assert(opens == closes);
        if (!wanted || step != SERVE)
            assert(!*debug_ip());
    }
    cancelled();
}
int main(void) {
    test_default_and_normal();
    puts("PASS: ADB default, normal transitions, address changes and reuse");
    test_config();
    puts("PASS: configuration rejection, maximum-length credentials and missing APIs");
    test_service();
    puts("PASS: rapid toggles, delayed/failed stops, start recovery and other transports");
    test_network();
    puts("PASS: stale network rejection, loss recovery, bounded retries and fallback checks");
    test_parser();
    puts("PASS: listener parsing, short reads, truncation, overflow and bind addresses");
    test_cancellation_and_stress();
    puts("PASS: cancellation at every transition and 10,000 fault/toggle events");
    assert(opens == closes);
    return 0;
}
