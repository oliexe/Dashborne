#ifndef DASHBORNE_WIDGET_H
#define DASHBORNE_WIDGET_H
#include "dashborne_extension.h"
#define DASHBORNE_WIDGET_ABI 1u
enum dashborne_widget_size { DBW_1X1, DBW_2X1, DBW_1X2, DBW_2X2,
                            DBW_3X2, DBW_4X2, DBW_5X2, DBW_6X2, DBW_SIZE_COUNT };
#define DBW_ALL_SIZES 255u
enum dashborne_widget_tab { DBW_STATS, DBW_ACTIONS, DBW_SPECIAL };
/* Categories are zero-based within their tab; see widget-example/README.md. */
struct dashborne_widget_view {
    unsigned int struct_size, instance;
    int width, height, editing, visible;
    void *window, *canvas, *title, *value, *status, *action;
    void *user;
};
struct dashborne_widget {
    unsigned int abi_version, struct_size;
    const char *id, *title, *description;
    unsigned int sizes, tab, category;
    /* Called on the UI thread. A view can be reused for another placement.
       create/destroy bracket each assignment, not the lifetime of its window. */
    int (*create)(struct dashborne_widget_view *);
    void (*update)(struct dashborne_widget_view *, unsigned long long microseconds);
    void (*draw)(struct dashborne_widget_view *, void *canvas);
    void (*click)(struct dashborne_widget_view *);
    void (*destroy)(struct dashborne_widget_view *);
    /* Dashboard pause: release caches or stop animation; create is not repeated
       on resume. update resumes when the dashboard is active again. */
    void (*suspend)(struct dashborne_widget_view *);
    /* Optional static PNG previews, indexed by dashborne_widget_size. Use the
       widget's aspect ratio; the picker fits each image without stretching.
       Missing entries/files use the core's size preview. Paths and strings
       must remain valid until unregister_widget. No live callbacks run here. */
    const char *preview_images[DBW_SIZE_COUNT];
};
/* ABI 1 descriptors built before picker previews remain supported. */
#define DASHBORNE_WIDGET_V1_SIZE __builtin_offsetof(struct dashborne_widget,preview_images)
struct dashborne_widgets {
    unsigned int abi_version, struct_size;
    int (*register_widget)(const struct dashborne_host *, const struct dashborne_widget *);
    int (*unregister_widget)(const struct dashborne_host *);
    void (*text)(void *control, const char *);
    void (*invalidate)(struct dashborne_widget_view *);
};
/* The runtime is built into Dashborne. This temporary dlopen reference is released
   immediately; never use the API after your extension has deactivated. */
static inline const struct dashborne_widgets *dashborne_widget_api(void) {
    extern void *dlopen(const char *,int),*dlsym(void *,const char *);
    extern int dlclose(void *);
    void *library=dlopen("/mnt/extsd/Dashborne/lib/libdashborne.so",2|4);
    if(!library)return (void *)0;
    const struct dashborne_widgets *(*get)(void)=(void *)0;
    *(void **)(&get)=dlsym(library,"dashborne_widgets_v1");
    const struct dashborne_widgets *api=get?get():(void *)0;
    dlclose(library);
    return api && api->abi_version==DASHBORNE_WIDGET_ABI && api->struct_size>=sizeof(*api)?api:(void *)0;
}
#endif
