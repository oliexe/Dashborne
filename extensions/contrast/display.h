#ifndef CONTRAST_DISPLAY_H
#define CONTRAST_DISPLAY_H
struct display_state {
    int baseline_contrast, baseline_luminance;
    int contrast, luminance, captured, touched, enabled;
};
int display_capture(struct display_state *s);
int display_apply(struct display_state *s, int contrast, int luminance);
int display_restore(struct display_state *s);
#endif
