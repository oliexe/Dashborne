#include "../data.h"
#include <assert.h>
#include <string.h>
int main(void) {
    unsigned char slow[0xd6]={0},before[sizeof(slow)];
    const unsigned int offsets[]={0xd1,0xd3,0xd0,0xd2,0xd4,0xd5};
    for(unsigned int mask=0;mask<64;++mask) {
        for(unsigned int i=0;i<6;++i)slow[offsets[i]]=(mask>>i)&1;
        memcpy(before,slow,sizeof(slow));
        struct readings r={.connected=1};vehicle_panel_read(&r,slow);
        assert(r.body_known==63 && r.body_open==mask && !memcmp(before,slow,sizeof(slow)));
        r.connected=0;vehicle_panel_read(&r,slow);assert(!r.body_known && !r.body_open);
        r.connected=r.demo=1;vehicle_panel_read(&r,slow);assert(!r.body_known && !r.body_open);
    }
    struct readings r={.connected=1};
    slow[0xd1]=255;vehicle_panel_read(&r,slow);
    assert(r.body_known==62 && !(r.body_open&1));
    vehicle_panel_read(&r,0);assert(!r.body_known && !r.body_open);
}
