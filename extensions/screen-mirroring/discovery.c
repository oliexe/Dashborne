#include "discovery.h"
/* One bounded opportunity per dashboard/widget opening. A later stock
   revocation must not cause a receiver restart loop. Time is monotonic. */
int discovery_step(struct mirroring_discovery *s, int widget, int manual_off,
                   int video_active, int projection_enabled, long now) {
    if(!widget) {
        int ended=s->phase==1;
        *s=(struct mirroring_discovery){0,0};
        return ended?DISCOVERY_END:0;
    }
    if(manual_off || video_active || now<0) {
        int ended=s->phase==1;
        s->phase=2;
        return ended?DISCOVERY_END:0;
    }
    if(s->phase==2)return 0;
    if(s->phase==0) {
        if(now>__LONG_MAX__-120) {s->phase=2;return 0;}
        s->phase=1;s->deadline=now+120;
        return DISCOVERY_HOLD|DISCOVERY_BEGIN|(projection_enabled?0:DISCOVERY_REQUEST);
    }
    if(now>=s->deadline || !projection_enabled) {
        s->phase=2;
        return DISCOVERY_END;
    }
    return DISCOVERY_HOLD;
}
