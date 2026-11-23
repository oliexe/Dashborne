#include <assert.h>
#include "../discovery.h"
int main(void) {
    struct mirroring_discovery s={0,0};
    assert(!discovery_step(&s,0,0,0,0,100));
    assert(discovery_step(&s,1,0,0,0,100)==(DISCOVERY_BEGIN|DISCOVERY_REQUEST|DISCOVERY_HOLD));
    assert(s.deadline==220);
    assert(discovery_step(&s,1,0,0,1,219)==DISCOVERY_HOLD);
    assert(discovery_step(&s,1,0,0,1,220)==DISCOVERY_END);
    assert(!discovery_step(&s,1,0,0,0,221)); /* No endless rediscovery. */
    discovery_step(&s,0,0,0,0,222);
    assert(discovery_step(&s,1,0,0,1,223)==(DISCOVERY_BEGIN|DISCOVERY_HOLD));
    assert(discovery_step(&s,1,0,0,0,224)==DISCOVERY_END); /* Stock revocation. */
    assert(!discovery_step(&s,1,0,0,0,225));
    s=(struct mirroring_discovery){0,0};
    discovery_step(&s,1,0,0,0,300);
    assert(discovery_step(&s,1,0,1,1,301)==DISCOVERY_END); /* Video takes over. */
    assert(!discovery_step(&s,1,0,0,1,302)); /* Disconnect releases wake hold. */
    s=(struct mirroring_discovery){0,0};
    discovery_step(&s,1,0,0,0,400);
    assert(discovery_step(&s,1,1,0,1,401)==DISCOVERY_END);
    assert(!discovery_step(&s,1,0,0,0,402)); /* Manual off isn't undone. */
    s=(struct mirroring_discovery){0,0};
    assert(!discovery_step(&s,1,1,0,0,500));
    s=(struct mirroring_discovery){0,0};
    assert(!discovery_step(&s,1,0,0,0,-1)); /* Clock failure. */
    s=(struct mirroring_discovery){0,0};
    assert(!discovery_step(&s,1,0,0,0,__LONG_MAX__-119));
    return 0;
}
