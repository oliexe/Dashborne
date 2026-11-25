#include "controller.h"
static int activate(const struct dashborne_host *h) {
    if(!h||h->abi_version!=DASHBORNE_EXTENSION_ABI||h->struct_size<sizeof(*h)||!h->owns_callback)return -1;
    if(display_controller_start(h)||display_window_bind(h))return -1;
    return h->register_extra(h,"display","Better Display Controls","Brightness, black level and day/night profiles.",display_window_open);
}
static int deactivate(void) {
    if(display_window_cleanup())return -1;
    return display_controller_stop();
}
DASHBORNE_EXTENSION_EXPORT const struct dashborne_extension *dashborne_extension_v1(void) {
    static const struct dashborne_extension e={DASHBORNE_EXTENSION_ABI,sizeof(e),"better-display-controls",activate,deactivate};return &e;
}
