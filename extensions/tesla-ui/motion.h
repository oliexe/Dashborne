#ifndef TESLA_UI_MOTION_H
#define TESLA_UI_MOTION_H
struct camera_motion {float position,from,target;unsigned long long start;int primed;};
void camera_target(struct camera_motion *,int drive,unsigned long long now,int immediate);
int camera_frame(struct camera_motion *,unsigned long long now);
#endif
