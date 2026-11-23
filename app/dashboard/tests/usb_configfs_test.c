#include <assert.h>
#include <stdio.h>
#include <string.h>
#define open fake_open
#define read fake_read
#define write fake_write
#define close fake_close
#define access fake_access
#define mkdir fake_mkdir
#define symlink fake_symlink
#define mount fake_mount
#define __errno_location fake_errno_location
#include "../usb_configfs.c"

static const char *scenario;
static int error_number, count, bindings, mounts;
static struct { char path[256], value[256]; } nodes[64];
static int is(const char *s) { return !strcmp(scenario, s); }
int *fake_errno_location(void) { return &error_number; }
static int index_of(const char *path) {
    for (int i = 0; i < count; ++i) if (!strcmp(nodes[i].path, path)) return i;
    return -1;
}
static int add(const char *path, const char *value) {
    int i = index_of(path);
    if (i < 0) { assert(count < 64); i = count++; snprintf(nodes[i].path, sizeof(nodes[i].path), "%s", path); }
    snprintf(nodes[i].value, sizeof(nodes[i].value), "%s", value); return i;
}
int fake_access(const char *path, int mode) { assert(mode == 0); return index_of(path) < 0 ? -1 : 0; }
int fake_open(const char *path, int flags, ...) {
    assert(flags == 0 || flags == 1);
    int i = index_of(path);
    if (flags && i < 0) {
        assert(!strncmp(path, GADGET "/", sizeof(GADGET)) && created);
        i = add(path, "");
    }
    return i < 0 ? -1 : i + 10;
}
int fake_read(int fd, void *out, unsigned int n) {
    assert(fd >= 10 && fd < count + 10);
    const char *value = nodes[fd - 10].value;
    unsigned int size = (unsigned int)strlen(value); if (size > n) size = n;
    memcpy(out, value, size); return (int)size;
}
int fake_write(int fd, const void *data, unsigned int n) {
    assert(fd >= 10 && fd < count + 10 && n < 256);
    const char *path = nodes[fd - 10].path;
    assert(!strncmp(path, GADGET "/", sizeof(GADGET)));
    if (is("attribute-fail") && !strcmp(path, GADGET "/idVendor")) { error_number = 13; return -1; }
    if (!strcmp(path, GADGET "/UDC")) {
        if (n > 1) {
            assert(index_of(FFS "/ep1") >= 0 && index_of(FFS "/ep2") >= 0);
            assert(n == strlen(CONTROLLER "\n") && !memcmp(data, CONTROLLER "\n", n));
            if (is("bind-timeout")) { error_number = 19; return -1; }
            ++bindings;
        } else if (is("stop-fail")) { error_number = 16; return -1; }
    }
    memcpy(nodes[fd - 10].value, data, n); nodes[fd - 10].value[n] = 0;
    return (int)n;
}
int fake_close(int fd) { assert(fd >= 10 && fd < count + 10); return 0; }
int fake_mkdir(const char *path, unsigned int mode) {
    assert(mode == 0755);
    assert(!strncmp(path, GADGET, sizeof(GADGET) - 1) || !strncmp(path, "/dev/usb-ffs", 12));
    if (is("mkdir-fail")) { error_number = 13; return -1; }
    add(path, ""); return 0;
}
int fake_symlink(const char *target, const char *path) {
    assert(!strcmp(target, GADGET "/functions/ffs.adb") && !strcmp(path, GADGET "/configs/c.1/ffs.adb"));
    add(path, target); return 0;
}
int fake_mount(const char *source, const char *target, const char *type, unsigned long flags, const void *options) {
    assert(!flags); ++mounts;
    if (!strcmp(type, "configfs")) {
        assert(!strcmp(source, "none") && !strcmp(target, CONFIG) && !options);
        if (is("config-mount-fail")) { error_number = 19; return -1; }
        add(CONFIG "/usb_gadget", "");
    } else {
        assert(!strcmp(type, "functionfs") && !strcmp(source, "adb") && !strcmp(target, FFS));
        assert(!strcmp(options, "uid=2000,gid=2000"));
        if (is("ffs-mount-fail")) { error_number = 19; return -1; }
        add(FFS "/ep0", "");
    }
    return 0;
}
static void logger(const char *text) { assert(text && *text); }

int main(int argc, char **argv) {
    assert(argc == 2); scenario = argv[1];
    add(UDC, ""); add(UDC "/function", is("busy-controller") ? "existing driver\n" : "\n");
    add(UDC "/state", "not-attached\n");
    if (is("old-gadget")) add(GADGET, "");
    assert(usb_configfs_available());
    int result = usb_configfs_prepare(logger);
    if (is("config-mount-fail") || is("ffs-mount-fail") || is("busy-controller") ||
        is("old-gadget") || is("attribute-fail") || is("mkdir-fail")) {
        assert(result == -1 && !bindings && failure);
        if (is("busy-controller")) assert(!mounts && !created);
    } else {
        assert(result == 0 && prepared && !bound);
        assert(!strcmp(nodes[index_of(GADGET "/strings/0x409/manufacturer")].value, "Dashborne\n"));
        assert(usb_configfs_bind() == 0 && !bindings);
        if (!is("endpoint-timeout")) { add(FFS "/ep1", ""); add(FFS "/ep2", ""); }
        if (is("endpoint-timeout") || is("bind-timeout")) {
            for (int i = 0; i < 13; ++i) assert(usb_configfs_bind() == 0);
            assert(usb_configfs_bind() == -1 && !bindings);
        } else {
            assert(usb_configfs_bind() == 1 && bindings == 1);
            assert(!strcmp(usb_configfs_status(), "ADB: waiting for USB host"));
            add(UDC "/state", "configured\n");
            assert(!strcmp(usb_configfs_status(), "ADB: USB configured"));
            if (is("stop-fail")) assert(usb_configfs_stop() == -1 && bound);
            else {
                assert(usb_configfs_stop() == 0 && !bound);
                assert(!strcmp(nodes[index_of(GADGET "/UDC")].value, "\n"));
                assert(usb_configfs_prepare(logger) == 0 && mounts == 2);
                assert(usb_configfs_bind() == 1 && bindings == 2);
            }
        }
    }
    printf("PASS: ConfigFS %s\n", scenario);
}
