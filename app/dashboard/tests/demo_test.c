#include <assert.h>
#include <stdio.h>
#include "../demo.c"
int main(void) {
    struct demo_state state;
    demo_init(&state);
    for (int i = 0; i < 10000; ++i) {
        demo_tick(&state);
        assert(state.frame < 8 && state.ticks < 160 && state.speed >= 70 && state.speed <= 74);
    }
    demo_toggle_pause(&state);
    unsigned int ticks = state.ticks, frame = state.frame;
    for (int i = 0; i < 100; ++i) demo_tick(&state);
    assert(state.ticks == ticks && state.frame == frame);
    demo_toggle_park(&state); assert(state.speed == 0 && state.parked);
    demo_toggle_pause(&state);
    demo_tick(&state); assert(state.speed == 0 && state.ticks == 0);
    demo_toggle_park(&state); assert(state.speed == 72 && !state.parked);
    demo_tick(&state); assert(state.ticks == 1);
    demo_toggle_pane(&state); assert(state.energy);
    demo_toggle_pane(&state); assert(!state.energy);
    puts("PASS: bounded demo playback, pause, park and pane switching");
}
