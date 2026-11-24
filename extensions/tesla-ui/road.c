#include "road.h"
void road_step(struct road_motion *motion,const struct cluster *c,unsigned long long now) {
    if(c->mode!=DRIVING) {*motion=(struct road_motion){0};return;}
    unsigned long long elapsed=motion->primed && now>=motion->last?now-motion->last:0;
    motion->last=now;motion->primed=1;
    /* Stop at invalid speed/unknown direction; never replay a stalled timer. */
    if(elapsed>250000 || c->data.speed<=0 || (c->data.gear!=2 && c->data.gear!=4))return;
    float metres_per_second=(float)c->data.speed*(c->data.mph?0.44704f:1.0f/3.6f);
    float distance=metres_per_second*(float)(unsigned int)elapsed/1000000.0f;
    motion->phase+=c->data.gear==2?-distance:distance;
    while(motion->phase>=12)motion->phase-=12;
    while(motion->phase<0)motion->phase+=12;
}
struct road_mark road_mark(float phase,int index) {
    if(index<0 || index>=ROAD_MARKS)return (struct road_mark){0};
    float distance=(float)(index%6)*12-phase;
    if(distance<0)distance+=72;
    float near=18/(18+distance),far=18/(22+distance);
    int yn=55+(int)(360*near),yf=55+(int)(360*far);
    int xn=(int)(210*near),xf=(int)(210*far);
    return (struct road_mark){index<6?980-xn-1:980+xf-1,yf,xn-xf+3,yn-yf};
}
enum road_zone road_zone(const struct cluster *c,unsigned int side) {
    if(c->mode!=DRIVING || (side!=SIDE_LEFT && side!=SIDE_RIGHT))return ZONE_OFF;
    int turn=side==SIDE_LEFT?c->data.left:c->data.right;
    if(c->data.blind_known & c->data.blind_active & side)
        return turn || (c->data.blind_urgent&side)?ZONE_ALERT:ZONE_BLIND;
    return turn?ZONE_TURN:ZONE_OFF;
}
