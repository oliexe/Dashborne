#ifndef DASHBORNE_DEMO_H
#define DASHBORNE_DEMO_H
struct demo_state { unsigned int ticks, frame; int speed, paused, parked, energy; };
void demo_init(struct demo_state *state);
void demo_tick(struct demo_state *state);
void demo_toggle_park(struct demo_state *state);
void demo_toggle_pause(struct demo_state *state);
void demo_toggle_pane(struct demo_state *state);
#endif
