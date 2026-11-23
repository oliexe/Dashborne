#include "dashborne_extension.h"
static const struct dashborne_host *host;
static int activate(const struct dashborne_host *api) {
    if (!api || api->abi_version != DASHBORNE_EXTENSION_ABI || api->struct_size < sizeof(*api))
        return -1;
    host = api;
    if (!host->resolve(host->official_library, "onStartupApp")) return -1;
    host->log(host, "lifecycle-example activated; official symbols available");
    return 0;
}
static int deactivate(void) {
    if (host) host->log(host, "lifecycle-example cleanup complete");
    host = (void *)0;
    return 0;
}
DASHBORNE_EXTENSION_EXPORT const struct dashborne_extension *dashborne_extension_v1(void) {
    static const struct dashborne_extension extension = {
        DASHBORNE_EXTENSION_ABI, sizeof(struct dashborne_extension),
        "lifecycle-example", activate, deactivate
    };
    return &extension;
}
