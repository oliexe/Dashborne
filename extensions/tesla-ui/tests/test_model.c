#include <assert.h>
#include <math.h>
#include "../model.h"
int main(void) {
    struct cluster c;cluster_reset(&c);
    struct readings r={.connected=1,.speed=72,.limit=80,.battery=68,.range=312,
        .temperature=21,.gear=4,.power_kw=34,.power_valid=1};
    cluster_update(&c,&r,100);assert(c.mode==DRIVING && c.history[59]==340 && c.count==1);
    r.power_kw=-18;cluster_update(&c,&r,100);assert(c.history[59]==340);
    cluster_update(&c,&r,101);assert(c.history[59]==-180 && c.history[58]==340);
    cluster_update(&c,&r,104);assert(c.history[58]==UNKNOWN && c.history[57]==UNKNOWN);
    r.gear=1;cluster_update(&c,&r,105);assert(c.mode==PARKED);
    r.charging=1;cluster_update(&c,&r,106);assert(c.mode==CHARGING);
    r.power_kw=NAN;r.nav_valid=1;r.nav_distance=INFINITY;
    r.battery=255;r.temperature=999;r.range=-1;r.limit=0;r.speed=-1;
    cluster_update(&c,&r,107);
    assert(!c.data.power_valid && !c.data.nav_valid && c.data.battery==UNKNOWN && c.data.speed==UNKNOWN);
    assert(c.data.temperature==UNKNOWN && c.data.range==UNKNOWN && c.data.limit==UNKNOWN);
    r.connected=0;cluster_update(&c,&r,108);
    assert(c.mode==WAITING && c.count==0 && c.data.gear==UNKNOWN && !c.sampled);
    r.connected=1;r.demo=1;cluster_update(&c,&r,109);assert(c.mode==WAITING);
    r.demo=0;r.gear=0;cluster_update(&c,&r,110);assert(c.mode==WAITING && c.data.speed==UNKNOWN && c.data.battery==UNKNOWN);
    r.gear=4;r.power_kw=1500;r.power_valid=1;
    cluster_update(&c,&r,111);cluster_update(&c,&r,200);assert(c.count==60 && c.history[58]==UNKNOWN);
    cluster_reset(&c);assert(!c.count && !c.sampled && c.history[59]==UNKNOWN);
}
