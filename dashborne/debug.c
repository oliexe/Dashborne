/* Bounded developer connection controller, advanced by the UI timer. */
#include "debug.h"
#include "debug_force.h"
#include "config.h"
#include "lan.h"
typedef __SIZE_TYPE__ usize;
extern void *dlsym(void *, const char *);
extern int open(const char *, int, ...), read(int, void *, unsigned int), close(int);
extern int socket(int, int, int), ioctl(int, unsigned long, ...);
extern int *__errno_location(void);
static void (*change_mode)(int), (*disconnect_wifi)(void);
static int (*get_mode)(void), (*is_connected)(void);
static void (*connect_wifi)(const void *, const void *);
static int (*get_property)(const char *, char *, const char *);
static int (*set_property)(const char *, const char *);
static void (*logger)(const char *);
static char ssid[33], password[64], address[16];
static const char *status = "Disabled";
static int active, wanted, owned_network, config_ok, start_pending;
static unsigned int ticks, stop_attempts, network_attempts, adb_attempts, stable, lost;
enum { STOP, PREPARE, OFF_WAIT, MODE_WAIT, SETTLE, CONNECT_WAIT, SERVE, FAILED };
static int step;
static int forced, network_only;
static void *network_init,*runtime_maintenance;
struct string_view_abi {
    const char *data;
    usize size;
    union {
        usize capacity;
        char local[16];
    } storage;
};
#if defined(__arm__)
_Static_assert(sizeof(struct string_view_abi) == 24, "ARM GNU string ABI");
#endif
static unsigned int length(const char *s) {
    unsigned int n = 0;
    while (s[n])
        ++n;
    return n;
}
static int equal(const char *a, const char *b) {
    while (*a && *a == *b) {
        ++a;
        ++b;
    }
    return *a == *b;
}
static void copy(char *a, const char *b) {
    while ((*a++ = *b++)) {
    }
}
static void wipe(void *p, unsigned int n) {
    volatile unsigned char *s = p;
    while (n--)
        *s++ = 0;
}
static void report(const char *message) {
    if (!equal(status, message)) {
        status = message;
        if (logger) {
            char line[160];
            unsigned int at = 0;
            const char *prefix = "debug: ";
            while (*prefix)
                line[at++] = *prefix++;
            while (*message && at + 2 < sizeof(line))
                line[at++] = *message++;
            line[at++] = '\n';
            line[at] = 0;
            logger(line);
        }
    }
}
static void fail(const char *message) {
    lan_reset();
    step = FAILED;
    ticks=0;
    address[0] = 0;
    report(message);
}
static int load_config(void) {
    wipe(ssid,sizeof(ssid));wipe(password,sizeof(password));
    return config_wifi(ssid,password);
}
static int properties(char *daemon, char *port) {
    daemon[0] = port[0] = 0;
    if (!get_property || !set_property)
        return 0;
    int a = get_property("init.svc.adbd", daemon, "");
    int b = get_property("service.adb.tcp.port", port, "");
    daemon[127] = port[127] = 0;
    return a >= 0 && b >= 0 && *daemon;
}
/* Linux UAPI ifreq/iwreq layouts, padded for host tests and ARM userspace. */
union request {
    unsigned long align;
    unsigned char bytes[40];
};
struct essid_request {
    char name[16];
    union {
        struct {
            void *pointer;
            unsigned short length, flags;
        } point;
        unsigned char pad[16];
    } value;
};
static int unsupported(void) {
    int e = *__errno_location();
    return e == 95 || e == 25;
}
static int network_address(char *ip) {
    ip[0] = 0;
    if (!get_mode || get_mode() != 2 || !is_connected())
        return 0;
    int fd = socket(2, 2, 0);
    if (fd < 0)
        return 0;
    union request r = {0};
    copy((char *)r.bytes, "wlan0");
    int ok = 0;
    if (ioctl(fd, 0x8913, &r) < 0 || (r.bytes[16] & 0x41) != 0x41)
        goto done; /* UP + RUNNING */
    r = (union request){0};
    copy((char *)r.bytes, "wlan0");
    if (ioctl(fd, 0x8b15, &r) == 0) { /* SIOCGIWAP: current association */
        unsigned int any = 0, all = 255;
        for (int i = 18; i < 24; ++i) {
            any |= r.bytes[i];
            all &= r.bytes[i];
        }
        if (!any || all == 255 || (r.bytes[18] & 1))
            goto done;
    } else if (!unsupported())
        goto done;
    char current[33] = {0};
    struct essid_request e = {0};
    copy(e.name, "wlan0");
    e.value.point.pointer = current;
    e.value.point.length = sizeof(current);
    if (ioctl(fd, 0x8b1b, &e) == 0) { /* SIOCGIWESSID */
        unsigned int count = e.value.point.length;
        if (count > sizeof(current))
            goto done;
        /* Some wireless drivers include the terminating NUL in the length. */
        if (count && !current[count - 1])
            --count;
        if (count > 32 || count != length(ssid))
            goto done;
        current[count] = 0;
        if (!equal(current, ssid))
            goto done;
    } else if (!unsupported())
        goto done;
    r = (union request){0};
    copy((char *)r.bytes, "wlan0");
    if (ioctl(fd, 0x8915, &r) < 0 || r.bytes[16] != 2 || r.bytes[17] || !r.bytes[20] ||
        r.bytes[20] == 127 || r.bytes[20] >= 224 || (r.bytes[20] == 169 && r.bytes[21] == 254))
        goto done;
    unsigned int at = 0;
    for (int i = 20; i < 24; ++i) {
        unsigned int v = r.bytes[i];
        if (v >= 100)
            ip[at++] = (char)('0' + v / 100);
        if (v >= 10)
            ip[at++] = (char)('0' + v / 10 % 10);
        ip[at++] = (char)('0' + v % 10);
        if (i != 23)
            ip[at++] = '.';
    }
    ip[at] = 0;
    ok = 1;
done:
    close(fd);
    return ok;
}
static int space(char c) { return c == ' ' || c == '\t'; }
static int token(const char **at, char *out, unsigned int cap) {
    unsigned int n = 0;
    while (space(**at))
        ++*at;
    while (**at && !space(**at)) {
        if (n + 1 >= cap)
            return 0;
        out[n++] = *(*at)++;
    }
    out[n] = 0;
    return n != 0;
}
static int socket_line(const char *line, const char *ip, int connected) {
    char ignored[96], local[48], remote[48], state[8];
    if (!token(&line, ignored, sizeof(ignored)) || !token(&line, local, sizeof(local)) ||
        !token(&line, remote, sizeof(remote)) || !token(&line, state, sizeof(state)) ||
        !equal(state, connected ? "01" : "0A"))
        return 0;
    if (connected && (equal(remote,"00000000:0000") || equal(remote,"00000000000000000000000000000000:0000")))
        return 0;
    unsigned int n = length(local);
    if ((n != 13 && n != 37) || !equal(local + n - 5, ":15B3"))
        return 0;
    int zero = 1;
    for (unsigned int i = 0; i < n - 5; ++i)
        if (local[i] != '0')
            zero = 0;
    if (zero)
        return 1; /* all interfaces; excludes loopback-only listeners */
    if (n != 13)
        return 0;
    unsigned int octets[4] = {0};
    int part = 0;
    for (; *ip; ++ip) {
        if (*ip == '.') {
            if (++part > 3)
                return 0;
        } else if (*ip >= '0' && *ip <= '9')
            octets[part] = octets[part] * 10 + (unsigned int)(*ip - '0');
        else
            return 0;
    }
    if (part != 3)
        return 0;
    const char *hex = "0123456789ABCDEF";
    for (int i = 0; i < 4; ++i)
        if (octets[3 - i] > 255 || local[2 * i] != hex[octets[3 - i] >> 4] ||
            local[2 * i + 1] != hex[octets[3 - i] & 15])
            return 0;
    return 1;
}
static int socket_file(const char *path, const char *ip, int connected) {
    int fd = open(path, 0);
    if (fd < 0)
        return 0;
    char buf[256], line[512];
    unsigned int at = 0;
    int n = 0, overflow = 0, found = 0;
    while (!found && (n = read(fd, buf, sizeof(buf))) > 0)
        for (int i = 0; i < n; ++i) {
            if (buf[i] == '\n') {
                line[at] = 0;
                if (!overflow && socket_line(line, ip, connected)) {
                    found = 1;
                    break;
                }
                at = 0;
                overflow = 0;
            } else if (at + 1 < sizeof(line))
                line[at++] = buf[i];
            else
                overflow = 1;
        }
    if (!found && n == 0 && at && !overflow) {
        line[at] = 0;
        found = socket_line(line, ip, connected);
    }
    close(fd);
    return found;
}
static int listener_file(const char *path,const char *ip) {return socket_file(path,ip,0);}
static void begin_network(void) {
    address[0] = 0;
    ticks = stable = lost = 0;
    if (network_attempts++ >= 3) {
        fail("Wi-Fi connection failed - toggle ADB to retry");
        return;
    }
    lan_reset();
    owned_network = 1;
    disconnect_wifi();
    change_mode(0);
    step = OFF_WAIT;
    report("Resetting developer Wi-Fi...");
}
static void poll_stop(void) {
    char daemon[128], port[128];
    if (!properties(daemon, port)) {
        fail("Cannot read ADB service state");
        return;
    }
    /* A queued start can still report stopped. Send the cancelling stop before
       accepting that state; property control requests are asynchronous. */
    if (start_pending && equal(daemon, "stopped") && equal(port, "5555")) {
        if (ticks) {
            --ticks;
            return;
        }
        if (stop_attempts++ >= 3) {
            fail("ADB did not stop - toggle to retry");
            return;
        }
        ticks = 2;
        if (set_property("ctl.stop", "adbd"))
            report("ADB stop failed; retrying...");
        else {
            start_pending = 0;
            report("Stopping ADB...");
        }
        return;
    }
    if (equal(daemon, "stopped")) {
        if (ticks) {
            --ticks;
            return;
        }
        if (!wanted && !network_only) {
            if (equal(port, "5555") && set_property("service.adb.tcp.port", "-1")) {
                if (++stop_attempts >= 3)
                    fail("ADB stopped; TCP setting could not be cleared");
                else
                    report("Clearing ADB TCP setting...");
                return;
            }
            active = 0;
            report("Disabled");
            return;
        }
        if (!config_ok) {
            fail("Missing or invalid Wi-Fi config on microSD");
            return;
        }
        if (!change_mode || !get_mode || !is_connected || !connect_wifi || !disconnect_wifi) {
            fail("Wi-Fi control unavailable");
            return;
        }
        step = PREPARE;
        ticks = 0;
        report("Preparing developer connection...");
        return;
    }
    if (!equal(port, "5555")) {
        if (!wanted) {
            active = 0;
            report("Disabled - another ADB transport is active");
        } else
            fail("ADB is using another transport");
        return;
    }
    if (ticks) {
        --ticks;
        return;
    }
    if (stop_attempts++ >= 3) {
        fail("ADB did not stop - toggle to retry");
        return;
    }
    ticks = 5;
    if (set_property("ctl.stop", "adbd"))
        report("ADB stop failed; retrying...");
    else
        report("Stopping ADB...");
}
void debug_bind(void *stock, void *runtime, void (*log)(const char *)) {
    logger = log;
#define BIND(handle, var, name) *(void **)(&var) = dlsym(handle, name)
    BIND(stock, runtime_maintenance, "_ZN13WdgDataSource20maintainRuntimeStateEv");
    BIND(stock, network_init, "_ZN3net4initEv");
    BIND(stock, change_mode, "_ZN3net11change_modeE10net_mode_e");
    BIND(stock, get_mode, "_ZN3net8get_modeEv");
    BIND(stock, is_connected, "_ZN3net4wifi12is_connectedEv");
    BIND(stock, connect_wifi,
         "_ZN3net4wifi7connectERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEES8_");
    BIND(stock, disconnect_wifi, "_ZN3net4wifi10disconnectEv");
    BIND(runtime, get_property, "_ZN16SystemProperties9getStringEPKcPcS1_");
    BIND(runtime, set_property, "_ZN16SystemProperties9setStringEPKcS1_");
#undef BIND
}
void debug_set_enabled(int enabled) {
    network_only=0;
    if(!enabled)(void)debug_set_forced(0);
    lan_reset();
    active = 1;
    wanted = !!enabled;
    step = STOP;
    address[0] = 0;
    ticks = stop_attempts = network_attempts = adb_attempts = stable = lost = 0;
    if (!wanted && owned_network && get_mode && get_mode() == 2 && disconnect_wifi)
        disconnect_wifi();
    owned_network = 0;
    wipe(password, sizeof(password));
    wipe(ssid, sizeof(ssid));
    config_ok = wanted ? load_config() : 0;
    if (!get_property || !set_property) {
        fail("ADB control unavailable");
        return;
    }
    poll_stop();
}
void debug_connect_wifi(void) {
    if(wanted && active && step!=FAILED)return;
    debug_set_enabled(0);network_only=1;active=1;config_ok=load_config();step=STOP;
    poll_stop();
}
void debug_release(void) {
    (void)debug_set_forced(0);
    lan_reset();
    active = wanted = owned_network = start_pending = network_only = 0;
    address[0] = 0;
    wipe(password, sizeof(password));
    wipe(ssid, sizeof(ssid));
}
int debug_set_forced(int enabled) {
    if(enabled) {
        if(!wanted || !config_ok || !debug_force_install(&change_mode,network_init,runtime_maintenance)) {
            report("Force ADB unavailable on this software");return 0;
        }
        forced=1;
        if(logger)logger("debug: Force ADB enabled\n");
    } else {
        if(!debug_force_remove(&change_mode)) {
            report("Force ADB cleanup failed - restart required");return 0;
        }
        forced=0;
    }
    return 1;
}
int debug_is_forced(void) {return forced;}
void debug_poll(void) {
    if (!active)return;
    if(step==FAILED) {
        if(forced && wanted && ++ticks>=60)debug_set_enabled(1);
        return;
    }
    if (step == STOP) {
        poll_stop();
        return;
    }
    if (!wanted && !network_only)
        return;
    char ip[16];
    if (step == PREPARE) {
        if (++ticks < 2)
            return;
        begin_network();
        return;
    }
    if (step == OFF_WAIT) {
        /* Allow the asynchronous driver teardown to finish before reloading it. */
        if (get_mode() == 0 && ++stable >= 3) {
            change_mode(2);step = MODE_WAIT;ticks = stable = 0;
            report("Waiting for Wi-Fi station mode...");
        } else {
            if (get_mode() != 0) stable = 0;
            if (++ticks >= 20) begin_network();
        }
        return;
    }
    if (step == MODE_WAIT) {
        if (get_mode() == 2) {
            disconnect_wifi();
            step = SETTLE;
            ticks = 0;
            report("Preparing Wi-Fi connection...");
        } else if (++ticks >= 20)
            begin_network();
        return;
    }
    if (step == SETTLE) {
        if (get_mode() != 2) {
            begin_network();
            return;
        }
        if (++ticks < 3)
            return;
        struct string_view_abi s = {ssid, length(ssid), {.capacity = length(ssid)}};
        struct string_view_abi p = {password, length(password), {.capacity = length(password)}};
        connect_wifi(&s, &p);
        step = CONNECT_WAIT;
        ticks = stable = 0;
        report("Connecting to developer Wi-Fi...");
        return;
    }
    if (step == CONNECT_WAIT) {
        if (network_address(ip)) {
            if (++stable >= 2) {
                step = SERVE;
                ticks = 0;
                report(network_only ? "Wi-Fi connected" : "Wi-Fi connected; starting ADB...");
            }
        } else
            stable = 0;
        if (++ticks >= 60)
            begin_network();
        return;
    }
    if (step != SERVE)
        return;
    if (!network_address(ip)) {
        address[0] = 0;
        stable = 0;
        lan_reset();
        report("Wi-Fi link lost; reconnecting...");
        if (++lost >= 3)
            begin_network();
        return;
    }
    int lan = lan_poll(ip);
    if (lan != 1) {
        address[0] = 0;
        report(lan < 0 ? lan_error() : "Verifying local network...");
        if (lan < 0) begin_network();
        return;
    }
    lost = 0;
    if(network_only){copy(address,ip);report("Wi-Fi connected");return;}
    char daemon[128], port[128];
    if (!properties(daemon, port)) {
        address[0] = 0;
        report("Cannot read ADB service state");
        return;
    }
    if (equal(daemon, "running"))
        start_pending = 0;
    if (equal(daemon, "running") && !equal(port, "5555")) {
        fail("ADB is using another transport");
        return;
    }
    if (equal(daemon, "running") &&
        (listener_file("/proc/net/tcp", ip) || listener_file("/proc/net/tcp6", ip))) {
        copy(address, ip);
        ticks = 0;
        report(socket_file("/proc/net/tcp",ip,1) || socket_file("/proc/net/tcp6",ip,1)
               ? "ADB client connected" : "ADB listening; LAN verified");
        if (++stable >= 30) {
            network_attempts = adb_attempts = 0;
            stable = 30;
        }
        return;
    }
    address[0] = 0;
    stable = 0;
    /* Do not enqueue repeated starts while init is processing the first one. */
    if (start_pending) {
        if (++ticks < 20) {
            report("Starting ADB...");
            return;
        }
        ticks = stop_attempts = 0;
        step = STOP;
        poll_stop();
        return;
    }
    if (++ticks < 5) {
        report(equal(daemon, "running") ? "Waiting for ADB listener..." : "Starting ADB...");
        return;
    }
    ticks = 0;
    if (adb_attempts++ >= 3) {
        fail("ADB listener failed - toggle to retry");
        return;
    }
    if (equal(daemon, "running") || !equal(daemon, "stopped")) {
        step = STOP;
        stop_attempts = 0;
        poll_stop();
        return;
    }
    if (set_property("service.adb.tcp.port", "5555") || set_property("ctl.start", "adbd"))
        report("ADB start failed; retrying...");
    else {
        start_pending = 1;
        report("Starting ADB...");
    }
}
const char *debug_status(void) { return status; }
const char *debug_ip(void) { return address; }

const char *debug_wifi_name(void) { return (wanted || network_only) && config_ok ? ssid : ""; }
