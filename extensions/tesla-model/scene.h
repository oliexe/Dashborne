#ifndef TESLA_MODEL_SCENE_H
#define TESLA_MODEL_SCENE_H
enum tm_gear { TM_UNKNOWN, TM_PARK, TM_REVERSE, TM_NEUTRAL, TM_DRIVE };
/* Driver's left/right, front doors first within each side. */
enum tm_panel { TM_FL, TM_RL, TM_FR, TM_RR, TM_FRUNK, TM_TRUNK, TM_PANEL_COUNT };
struct tm_reading {
    int connected, gear, speed, left, right;
    unsigned int known, open;
    int lights_known, low_beam, high_beam;
};
/* Fixed-point animation retains sub-frame progress when a target changes. */
struct tm_motion { int value, from, target; unsigned int elapsed, duration; };
struct tm_scene {
    struct tm_reading data;
    int angle, panels[TM_PANEL_COUNT], road;
    unsigned long long tick, road_clock;
    int initialized;
    struct tm_motion camera, panel_motion[TM_PANEL_COUNT];
};
struct tm_layer { const char *name; int frame, x, y, w, h; };
struct tm_lamp {int x0,y0,x1,y1;unsigned int color;};
#define TM_LAYERS 16
/* Virtual canvas: 510 × 458. frame=-1 means a single PNG, not a sequence. */
int tm_step(struct tm_scene *, const struct tm_reading *, unsigned long long now);
int tm_layers(const struct tm_scene *, struct tm_layer out[TM_LAYERS]);
int tm_lamps(const struct tm_scene *,struct tm_lamp out[4]);
#endif
