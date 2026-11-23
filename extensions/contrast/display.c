/* Volatile display-engine controls, matching the installed hardware ABI. */
#include "display.h"
extern int open(const char *, int, ...);
extern int close(int);
extern int ioctl(int, unsigned long, ...);
#define GET_CONTRAST 0x193u
#define SET_CONTRAST 0x192u
#define GET_LUMINANCE 0x191u
#define SET_LUMINANCE 0x190u
static int command(unsigned int op, int value) {
    int fd = open("/dev/disp", 2);
    if (fd < 0) return -1;
    unsigned long args[4] = {0, (unsigned long)value, 0, 0};
    int result = ioctl(fd, op, args);
    close(fd);
    return result;
}
static int valid(int n) { return n >= 0 && n <= 100; }
int display_capture(struct display_state *s) {
    if (s->captured) return 0;
    int c = command(GET_CONTRAST, 0), l = command(GET_LUMINANCE, 0);
    if (!valid(c) || !valid(l)) return -1;
    *s = (struct display_state){c, l, c, l, 1, 0, 0};
    return 0;
}
int display_restore(struct display_state *s) {
    if (!s->touched) { s->enabled = 0; return 0; }
    int a = command(SET_CONTRAST, s->baseline_contrast);
    int b = command(SET_LUMINANCE, s->baseline_luminance);
    if (a < 0 || b < 0 || command(GET_CONTRAST, 0) != s->baseline_contrast ||
        command(GET_LUMINANCE, 0) != s->baseline_luminance) return -1;
    s->touched = 0; s->enabled = 0;
    return 0;
}
int display_apply(struct display_state *s, int contrast, int luminance) {
    if (!s->captured || !valid(contrast) || !valid(luminance)) return -1;
    s->touched = 1;
    if (command(SET_CONTRAST, contrast) < 0 || command(SET_LUMINANCE, luminance) < 0 ||
        command(GET_CONTRAST, 0) != contrast || command(GET_LUMINANCE, 0) != luminance) {
        return display_restore(s) ? -2 : -1;
    }
    s->contrast = contrast; s->luminance = luminance; s->enabled = 1;
    return 0;
}
