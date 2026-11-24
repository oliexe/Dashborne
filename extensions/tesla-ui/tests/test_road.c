#include <assert.h>
#include <math.h>
#include "../road.h"
int main(void) {
    struct cluster c;cluster_reset(&c);
    struct readings r={.connected=1,.gear=4,.speed=36};
    cluster_update(&c,&r,1);struct road_motion m={0};
    road_step(&m,&c,1000000);road_step(&m,&c,1100000);
    assert(fabsf(m.phase-1)<.0001f);
    r.mph=1;r.speed=10;cluster_update(&c,&r,1);
    road_step(&m,&c,1200000);assert(fabsf(m.phase-1.44704f)<.0001f);
    r.gear=2;cluster_update(&c,&r,1);road_step(&m,&c,1300000);assert(fabsf(m.phase-1)<.0001f);
    r.gear=3;cluster_update(&c,&r,1);road_step(&m,&c,1400000);assert(fabsf(m.phase-1)<.0001f);
    r.gear=4;r.speed=-1;cluster_update(&c,&r,1);road_step(&m,&c,1500000);assert(fabsf(m.phase-1)<.0001f);
    r.speed=100;cluster_update(&c,&r,1);
    road_step(&m,&c,2500000);road_step(&m,&c,1000000);assert(fabsf(m.phase-1)<.0001f);
    r.speed=400;cluster_update(&c,&r,1);
    for(int i=1;i<10000;++i) {road_step(&m,&c,1000000ull+i*16000);assert(m.phase>=0 && m.phase<12);}
    for(int phase=0;phase<1200;++phase)for(int i=0;i<ROAD_MARKS;++i) {
        struct road_mark mark=road_mark((float)phase/100,i);
        assert(mark.width>0 && mark.height>0 && mark.x>=680 && mark.x+mark.width<=1280);
        assert(mark.y>=55 && mark.y+mark.height<=415);
    }
    assert(!road_mark(0,-1).width && !road_mark(0,ROAD_MARKS).height);
    r.blind_known=SIDE_LEFT;r.blind_active=SIDES_ALL;r.blind_urgent=SIDE_RIGHT;
    cluster_update(&c,&r,1);
    assert(c.data.blind_active==SIDE_LEFT && !c.data.blind_urgent);
    assert(road_zone(&c,SIDE_LEFT)==ZONE_BLIND && road_zone(&c,SIDE_RIGHT)==ZONE_OFF);
    r.left=1;cluster_update(&c,&r,1);assert(road_zone(&c,SIDE_LEFT)==ZONE_ALERT);
    r.blind_known=0;cluster_update(&c,&r,1);assert(road_zone(&c,SIDE_LEFT)==ZONE_TURN);
    r.left=0;r.right=1;cluster_update(&c,&r,1);assert(road_zone(&c,SIDE_RIGHT)==ZONE_TURN);
    r.gear=1;cluster_update(&c,&r,1);road_step(&m,&c,200000000);
    assert(!m.primed && !m.phase && road_zone(&c,SIDE_RIGHT)==ZONE_OFF);
    r.gear=4;r.connected=0;r.blind_known=r.blind_active=r.blind_urgent=SIDES_ALL;
    cluster_update(&c,&r,1);
    assert(!c.data.blind_known && !c.data.blind_active && !c.data.blind_urgent);
    assert(road_zone(&c,SIDE_RIGHT)==ZONE_OFF && road_zone(&c,0)==ZONE_OFF);
}
