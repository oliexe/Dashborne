#ifndef VEHICLE_WIDGET_SCENE_H
#define VEHICLE_WIDGET_SCENE_H
#include "../tesla-ui/motion.h"

/* The widget adapter owns these bitmaps. All calls stay on the UI thread.
   load replaces one slot; unload must also release partially loaded objects. */
#define VEHICLE_BITMAP_SLOTS 77
struct vehicle_scene_ops {
    int (*load)(void *context,int slot,const char *name);
    void (*unload)(void *context,int slot);
    void (*invalidate)(void *context);
    void (*draw)(void *context,int slot,int x,int y,int width,int height);
};
struct vehicle_scene {
    struct vehicle_scene_ops ops;
    void *context;
    struct camera_motion camera;
    int assets[VEHICLE_BITMAP_SLOTS];
    int active,queued,frame,body,body_view,requested,group,group_view;
    int connected,panels_known,body_failed,target,dirty;
    float position,from;
    unsigned long long started,next_frame;
};
int vehicle_scene_open(struct vehicle_scene *,const struct vehicle_scene_ops *,void *context);
void vehicle_scene_close(struct vehicle_scene *);
void vehicle_scene_update(struct vehicle_scene *,int connected,int gear,
                          unsigned int known,unsigned int open,unsigned long long now);
void vehicle_scene_tick(struct vehicle_scene *,unsigned long long now);
void vehicle_scene_draw(struct vehicle_scene *);
#endif
