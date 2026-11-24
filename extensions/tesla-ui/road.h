#ifndef TESLA_UI_ROAD_H
#define TESLA_UI_ROAD_H
#include "model.h"
#define ROAD_ID 603000
#define ROAD_MARKS 12
#define ROAD_CONTROLS (4+ROAD_MARKS)
struct road_motion {float phase;unsigned long long last;int primed;};
struct road_mark {int x,y,width,height;};
enum road_zone {ZONE_OFF,ZONE_TURN,ZONE_BLIND,ZONE_ALERT};
void road_step(struct road_motion *,const struct cluster *,unsigned long long now);
struct road_mark road_mark(float phase,int index);
enum road_zone road_zone(const struct cluster *,unsigned int side);
#endif
