#include "demo.h"
void demo_init(struct demo_state *s) {
    s->ticks = s->frame = 0;
    s->speed = 72; s->paused = s->parked = s->energy = 0;
}
void demo_tick(struct demo_state *s) {
    static const int speeds[] = {72,72,73,73,74,74,73,72,71,70,70,71,72,72,73,72};
    if (s->paused || s->parked) return;
    s->ticks = (s->ticks + 1) % 160;
    s->frame = (s->frame + 1) % 8;
    s->speed = speeds[s->ticks / 10];
}
void demo_toggle_park(struct demo_state *s) {
    s->parked = !s->parked;
    s->speed = s->parked ? 0 : 72;
    s->ticks = s->frame = 0;
}
void demo_toggle_pause(struct demo_state *s) { s->paused = !s->paused; }
void demo_toggle_pane(struct demo_state *s) { s->energy = !s->energy; }
