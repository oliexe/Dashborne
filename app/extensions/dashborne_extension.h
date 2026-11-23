#ifndef DASHBORNE_EXTENSION_H
#define DASHBORNE_EXTENSION_H
/* Stable C interface. Build plugins for ARMv7 Linux, hard-float, as shared objects. */
#define DASHBORNE_EXTENSION_ABI 1u
#define DASHBORNE_EXTENSION_EXPORT __attribute__((visibility("default")))
struct dashborne_host {
    unsigned int abi_version, struct_size;
    const char *extension_directory;
    void *official_library, *ui_runtime;
    void (*log)(const struct dashborne_host *, const char *message);
    void *(*resolve)(void *library, const char *symbol);
    /* Activation only. Paths are relative to this extension and absolute /res/,
       respectively. The engine journals the temporary read-only file mount. */
    int (*overlay_file)(const struct dashborne_host *, const char *source, const char *target);
    /* Optional: select an activity registered by this plugin instead of the
       official startup activity. Only one selected plugin may claim it. */
    int (*set_startup_activity)(const struct dashborne_host *, const char *name);
};
struct dashborne_extension {
    unsigned int abi_version, struct_size;
    const char *id;
    /* Runs on the UI thread before the official startup activity is opened.
       Zero means success. Keep this callback short; own long-running work. */
    int (*activate)(const struct dashborne_host *);
    /* Undo hooks, unregister callbacks and join workers, including after a
       partially failed activate. Zero means safe to unload. */
    int (*deactivate)(void);
};
/* Export a getter with this exact name and signature. Keep the descriptor and
   callbacks alive until deactivate completes; the host struct stays valid. */
DASHBORNE_EXTENSION_EXPORT const struct dashborne_extension *dashborne_extension_v1(void);
#endif
