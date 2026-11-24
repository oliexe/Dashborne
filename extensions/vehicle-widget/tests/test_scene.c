#include "../scene.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct fixture {int slots[VEHICLE_BITMAP_SLOTS],loads,live,peak,queued,draws,fail;};
static int load(void *context,int slot,const char *name) {
    struct fixture *f=context;assert(!f->slots[slot] && strstr(name,".png"));
    ++f->loads;if(f->loads==f->fail)return 0;
    f->slots[slot]=1;++f->live;if(f->live>f->peak)f->peak=f->live;return 1;
}
static void unload(void *context,int slot) {
    struct fixture *f=context;if(f->slots[slot]){f->slots[slot]=0;--f->live;}
}
static void invalidate(void *context) {struct fixture *f=context;++f->queued;}
static void draw(void *context,int slot,int x,int y,int w,int h) {
    struct fixture *f=context;assert(f->slots[slot] && x>=0 && y>=0 && w>0 && h>0 && x+w<=600 && y+h<=360);++f->draws;
}
int main(void) {
    struct vehicle_scene_ops ops={load,unload,invalidate,draw};
    struct fixture f={0};struct vehicle_scene s={0};
    assert(!vehicle_scene_open(&s,&ops,&f));assert(f.loads==65 && f.live==65);
    assert(vehicle_scene_open(&s,&ops,&f)==-1);
    vehicle_scene_update(&s,1,1,63,0,1000000);vehicle_scene_draw(&s);assert(s.frame==0);
    vehicle_scene_update(&s,1,4,63,0,2000000);
    int queued=f.queued;
    for(int i=1;i<130;++i)vehicle_scene_tick(&s,2000000+i*10000ull);
    assert(f.queued<=queued+1); /* One pending repaint even when drawing stalls. */
    vehicle_scene_draw(&s);assert(s.frame==64 && f.loads==65);
    for(int drive=0;drive<2;++drive)for(int mask=0;mask<64;++mask) {
        unsigned long long now=5000000+(drive*64+mask)*3000000ull;
        vehicle_scene_update(&s,1,drive?4:1,63,0,now);
        vehicle_scene_tick(&s,now+1200000);vehicle_scene_draw(&s);
        vehicle_scene_update(&s,1,drive?4:1,63,(unsigned int)mask,now+1300000);
        vehicle_scene_tick(&s,now+1600000);vehicle_scene_draw(&s);
        vehicle_scene_tick(&s,now+1900000);vehicle_scene_draw(&s);
        assert(s.body==mask && s.panels_known && f.peak<=77);
    }
    vehicle_scene_update(&s,1,1,63,1,400000000);vehicle_scene_tick(&s,401000000);
    vehicle_scene_update(&s,1,1,1,1,402000000);vehicle_scene_draw(&s);
    assert(!s.panels_known && !s.body && f.live==65);
    vehicle_scene_update(&s,1,4,63,63,403000000);vehicle_scene_tick(&s,404000000);
    vehicle_scene_update(&s,0,4,63,63,405000000);vehicle_scene_draw(&s);
    assert(!s.connected && !s.panels_known && !s.body && s.frame==0 && f.live==65);
    vehicle_scene_close(&s);assert(f.live==0);
    int count=f.draws;queued=f.queued;
    vehicle_scene_tick(&s,406000000);vehicle_scene_draw(&s);vehicle_scene_close(&s);
    assert(f.draws==count && f.queued==queued);
    for(int failure=1;failure<=65;++failure) {
        f=(struct fixture){.fail=failure};
        assert(vehicle_scene_open(&s,&ops,&f)==-1 && !s.active && !f.live);
    }
    f=(struct fixture){.fail=66};assert(!vehicle_scene_open(&s,&ops,&f));
    vehicle_scene_update(&s,1,1,63,1,500000000);count=f.draws;
    vehicle_scene_draw(&s);assert(f.draws==count && s.body_failed);
    vehicle_scene_close(&s);assert(!f.live);
    puts("Vehicle widget scene: transitions, 128 panel poses, bounded cache, queueing and cleanup passed");
}
