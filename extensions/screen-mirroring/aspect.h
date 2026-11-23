#ifndef MIRROR_ASPECT_H
#define MIRROR_ASPECT_H
enum { MIRROR_ASPECT_COUNT=5 };
/* LayoutPosition is four consecutive signed 32-bit fields in the guarded runtime. */
struct mirror_rect { int left,top,width,height; };
_Static_assert(sizeof(struct mirror_rect)==16,"Native LayoutPosition ABI");
static inline int aspect_index(int value) {
    return value>=0 && value<MIRROR_ASPECT_COUNT?value:0;
}
static inline const char *aspect_label(int value) {
    static const char *const labels[]={"Full width","16:9","4:3",
                                      "9:16","9:19.5"};
    return labels[aspect_index(value)];
}
static inline struct mirror_rect aspect_rect(int value) {
    /* Even widths suit the video plane; every preset uses the full display height. */
    static const int widths[]={1920,854,640,270,222};
    int width=widths[aspect_index(value)];
    return (struct mirror_rect){(1920-width)/2,0,width,480};
}
static inline struct mirror_rect aspect_physical_rect(int value) {
    struct mirror_rect r=aspect_rect(value);
    return (struct mirror_rect){480-r.top-r.height,r.left,r.height,r.width};
}
#endif
