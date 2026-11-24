#include <assert.h>
#include "../motion.h"
int main(void) {
    struct camera_motion m={0};
    camera_target(&m,0,1000000,0);assert(camera_frame(&m,1000000)==0);
    camera_target(&m,1,2000000,0);
    int previous=0;
    for(int i=0;i<=110;++i) {
        int f=camera_frame(&m,2000000+i*10000ull);
        assert(f>=previous && f<=64);previous=f;
    }
    assert(previous==64);
    camera_target(&m,0,4000000,0);assert(camera_frame(&m,4000000)==64);
    int midway=camera_frame(&m,4500000);assert(midway>20 && midway<45);
    camera_target(&m,1,4500000,0);assert(camera_frame(&m,4500000)==midway);
    assert(camera_frame(&m,6000000)==64);
    /* Stall skips stale frames; clock rollback settles instead of overflowing. */
    camera_target(&m,0,7000000,0);assert(camera_frame(&m,12000000)==0);
    camera_target(&m,1,13000000,0);assert(camera_frame(&m,1)==64);
    camera_target(&m,0,14000000,1);assert(camera_frame(&m,14000000)==0);
    return 0;
}
