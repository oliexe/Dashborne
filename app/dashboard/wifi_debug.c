/* Use the installed application's station-mode workflow and Wi-Fi manager. */
#include "wifi_debug.h"
typedef __SIZE_TYPE__ usize;
extern void *dlsym(void *, const char *);
extern int open(const char *, int, ...);
extern int read(int, void *, unsigned int);
extern int close(int);
extern int socket(int, int, int);
extern int ioctl(int, unsigned long, ...);

#ifndef WIFI_CONFIG_PATH
#define WIFI_CONFIG_PATH "/mnt/extsd/Dashborne/wifi.conf"
#endif
#ifndef WIFI_TIMEOUT_STATUS
#define WIFI_TIMEOUT_STATUS "Wi-Fi: no address; tap Wi-Fi to retry"
#endif
#define CONFIG WIFI_CONFIG_PATH
static void (*change_mode)(int);
static int (*get_mode)(void), (*is_connected)(void);
static void (*connect_wifi)(const void *, const void *);
static void (*log_event)(const char *);
static char ssid[33], password[64], ip[16], status[80];
static const char *error;
static unsigned int ticks;
static int requested, started, available;

/* Const-reference views of the GNU C++11 string ABI used by the installed
   application: pointer, length, then capacity/16-byte SSO union. The callee
   copies these strings; the adapter never asks it to destroy borrowed storage. */
struct string_view_abi { const char *data; usize size; union { usize capacity; char local[16]; } storage; };
#if defined(__arm__)
_Static_assert(sizeof(struct string_view_abi) == 24, "ARM GNU string ABI");
#endif
static unsigned int length(const char *s) { unsigned int n = 0; while (s[n]) ++n; return n; }
static void copy(char *dst, const char *src) { while ((*dst++ = *src++)) {} }

static int load_config(void) {
    char raw[100]; unsigned int used = 0;
    int fd = open(CONFIG, 0);
    if (fd < 0) return 0;
    int n = 0;
    while (used < sizeof(raw) && (n = read(fd, raw + used, sizeof(raw) - used)) > 0) used += (unsigned int)n;
    (void)close(fd);
    if (used == sizeof(raw) || n < 0) return 0;
    unsigned int split = 0;
    while (split < used && raw[split] != '\n') ++split;
    if (split < 1 || split > 32 || used < split + 10 || raw[used - 1] != '\n') return 0;
    unsigned int passlen = used - split - 2;
    if (passlen < 8 || passlen > 63) return 0;
    for (unsigned int i = 0; i < used - 1; ++i)
        if (!raw[i] || raw[i] == '\r' || (raw[i] == '\n' && i != split)) return 0;
    for (unsigned int i = 0; i < split; ++i) ssid[i] = raw[i];
    ssid[split] = 0;
    for (unsigned int i = 0; i < passlen; ++i) password[i] = raw[split + 1 + i];
    password[passlen] = 0;
    for (unsigned int i = 0; i < sizeof(raw); ++i) ((volatile char *)raw)[i] = 0;
    return 1;
}

void wifi_debug_bind(void *stock, void (*logger)(const char *)) {
    log_event = logger;
#define BIND(var, name) *(void **)(&var) = dlsym(stock, name)
    BIND(change_mode, "_ZN3net11change_modeE10net_mode_e");
    BIND(get_mode, "_ZN3net8get_modeEv");
    BIND(is_connected, "_ZN3net4wifi12is_connectedEv");
    BIND(connect_wifi, "_ZN3net4wifi7connectERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEES8_");
#undef BIND
    available = change_mode && get_mode && is_connected && connect_wifi;
    if (!available) error = "Wi-Fi: runtime API unavailable";
}

void wifi_debug_retry(void) {
    if (!available) return;
    error = (const char *)0;
    started = 0; ip[0] = 0;
    if (!load_config()) { error = "Wi-Fi: missing or invalid SD config"; return; }
    /* Mode 2 is station mode in the inspected stock connect/is_connected API.
       change_mode queues work on the stock networking thread, including DHCP. */
    change_mode(2);
    ticks = 0; requested = 0; started = 1;
    log_event("wifi: station mode requested\n");
}

static void update_ip(void) {
    ip[0] = 0;
    if (!is_connected()) return;
    /* Linux ifreq uses a 16-byte interface name, followed by sockaddr.
       Extra padding accommodates both 32-bit and 64-bit host test layouts. */
    union { unsigned long align; unsigned char bytes[40]; } req = {0};
    copy((char *)req.bytes, "wlan0");
    int fd = socket(2, 2, 0); /* AF_INET, SOCK_DGRAM */
    if (fd < 0) return;
    int result = ioctl(fd, 0x8915, &req); /* SIOCGIFADDR */
    (void)close(fd);
    if (result < 0 || req.bytes[16] != 2 || req.bytes[17] || !req.bytes[20] || req.bytes[20] == 127) return;
    unsigned int at = 0;
    for (unsigned int i = 20; i < 24; ++i) {
        unsigned int v = req.bytes[i];
        if (v >= 100) ip[at++] = (char)('0' + v / 100);
        if (v >= 10) ip[at++] = (char)('0' + (v / 10) % 10);
        ip[at++] = (char)('0' + v % 10);
        if (i != 23) ip[at++] = '.';
    }
    ip[at] = 0;
}

void wifi_debug_poll(void) {
    if (!available || !started) return;
    update_ip();
    if (*ip && requested) { error = (const char *)0; return; }
    if (ticks < 91) ++ticks;
    if (!requested && ticks >= 5 && get_mode() == 2) {
        struct string_view_abi s = {ssid, length(ssid), {.capacity = length(ssid)}};
        struct string_view_abi p = {password, length(password), {.capacity = length(password)}};
        connect_wifi(&s, &p);
        requested = 1;
        log_event("wifi: connection requested\n");
    }
    if (ticks >= 90) error = WIFI_TIMEOUT_STATUS;
}

const char *wifi_debug_ip(void) { return ip; }
const char *wifi_debug_status(void) {
    if (error) return error;
    if (*ip) { copy(status, "Wi-Fi: "); copy(status + 7, ip); return status; }
    return started ? "Wi-Fi: connecting..." : "Wi-Fi: ready";
}
