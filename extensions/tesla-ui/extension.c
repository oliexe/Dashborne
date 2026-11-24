#include "window.h"
#include "source.h"
static const struct dashborne_host *host;
static int activate(const struct dashborne_host *api) {
    if(!api || api->abi_version!=DASHBORNE_EXTENSION_ABI || api->struct_size<sizeof(*api) || host ||
       !api->resolve || !api->register_activity || !api->register_extra)return -1;
    if(source_bind(api)) {api->log(api,"tesla-ui: unsupported app or missing data interface");return -1;}
    host=api;
    if(cluster_window_bind(api))return -1;
    return api->register_extra(api,"cluster","Tesla UI",
        "Model S instrument view with live vehicle data and energy history.",cluster_window_open);
}
static int deactivate(void) {
    if(cluster_window_cleanup())return -1;
    host=0;return 0;
}
DASHBORNE_EXTENSION_EXPORT const struct dashborne_extension *dashborne_extension_v1(void) {
    static const struct dashborne_extension extension={DASHBORNE_EXTENSION_ABI,sizeof(extension),"tesla-ui",activate,deactivate};
    return &extension;
}
