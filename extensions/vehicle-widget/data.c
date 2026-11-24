#include "data.h"
/* Reuse the guarded, read-only connection and gear adapter. */
#include "../tesla-ui/source.c"
void vehicle_read(struct readings *r) {
    source_read(r);
    vehicle_panel_read(r,slow);
}
