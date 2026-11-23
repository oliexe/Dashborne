#ifndef SCREEN_MIRRORING_DISCOVERY_H
#define SCREEN_MIRRORING_DISCOVERY_H
struct mirroring_discovery { int phase; long deadline; };
enum { DISCOVERY_HOLD=1, DISCOVERY_REQUEST=2, DISCOVERY_BEGIN=4, DISCOVERY_END=8 };
int discovery_step(struct mirroring_discovery *, int widget, int manual_off,
                   int video_active, int projection_enabled, long now);
#endif
