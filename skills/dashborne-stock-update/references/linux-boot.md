# Linux image and boot-menu compatibility

Audit the full system before treating Dashborne's boot menu as compatible. An unchanged stock app version or a successful extension build does not establish Linux compatibility.

## Establish coverage

Identify every partition/component in the offered update: bootloader, kernel, device tree, rootfs, application/resources, ancillary firmware, update scripts and partition metadata. Inspect all components actually present. Record which are absent, which a vendor update may leave unchanged, and the evidence for that conclusion. Do not infer unchanged base Linux solely from a resources-only image. For multipart or different containers, inspect their real format instead of relaxing the single-entry extractor's bounds checks.

When a complete image is available, extract its rootfs and inspect kernel/device-tree metadata and updater scripts statically. Compare to an identified working baseline. Do not run target scripts on the Mac or execute a vendor installer. Treat an encrypted/unrecognized partition as unknown, not compatible.

If no full Linux artifact is exposed, state that limitation. With an available DASH, collect a read-only base-system snapshot using ADB: kernel version and command line, mounts, partition map, init configuration, relevant runtime libraries, dynamic linker, startup executables, and device-node information. Before the update this is baseline evidence, not evidence of the new system. After an independently requested update it can establish target evidence. A selected-file snapshot is not a complete image; retain a missing-evidence list. Do not dump arbitrary partitions based on a remembered device-node number. An actual raw-image acquisition needs a verified partition map and an appropriate read-only capture method; never invoke a flasher to obtain it.

## Trace the actual boot chain

The expected chain is Linux/init → SD mounted at `/mnt/extsd` → stock `/bin/zkgui` / FlyThings runtime → card-root `EasyUI.cfg` selecting `/mnt/extsd/Dashborne/lib/libdashborne.so` → Dashborne splash/configuration → stock UI handoff. Reconfirm that path from the target's startup code/configuration. Check SD discovery timing, fallback config paths, partition/filesystem support, mount flags, permissions and whether the OS still accepts a card-hosted startup library.

Use `dashborne/build.sh` and the built ELF as the dependency source of truth. Today the loader is ARMv7, ARM state, hard-float/VFPv3-D16 and links to absolute dependencies `/res/lib/libzkgui.so` and `/lib/libz.so.1`; it opens `/lib/libeasyui.so`. Audit their transitive `DT_NEEDED` dependencies, symbol versions, C/C++ ABI, linker behavior and imported functions. Use `readelf`/LLVM tools, not `ldd` on an untrusted target executable.

A linker failure can prevent Dashborne code from running at all, so its “Some extensions failed to load” warning cannot cover that case. The loader/runtime guard must be updated only after confirming the new ABI; do not merely replace its CRC. If the new OS removes SD startup support, report a boot blocker and propose an approach instead of claiming extension-only changes solve it.

## System dependency matrix

| Area | Check |
| --- | --- |
| Kernel and boot | Architecture, boot arguments, device tree, partition and mount changes, SD recognition and timing |
| Dynamic loading | `/bin/zkgui`, linker/libc/libstdc++/zlib, app and EasyUI dependencies, symbol versions, calling conventions and vtables |
| Display/touch | Framebuffer/device nodes, format and dimensions, rotation, input event devices and permissions, rendering/runtime lifecycle |
| Networking and ADB | Wi-Fi driver/interface changes, DHCP and hotspot ownership, `/bin/adbd`, init/property service names and startup privileges |
| Extension engine | `dlopen`, bind-mount support, writable SD/temp storage, code protection/cache flush operations and cleanup |
| Recovery | Watchdog's ARM syscall assumptions, process/heartbeat observation, available reboot path, logging and recovery into configuration |
| Config and updates | `EasyUI.cfg` discovery, stock configuration migration, update scripts and whether they rewrite mount/startup behavior |

Use the source to discover additional dependencies rather than treating this table as complete. Record old/new hashes for system executables/libraries and inspect changes affecting actual imports/hooks. For static coverage, verify dependency closure and required interfaces. For runtime coverage, test cold boot, display and touch, configuration persistence, ADB enable/disable, stock handoff, extension failure fallback and recovery on the new Linux build.

Missing base-system evidence is a boot-compatibility uncertainty even when the app port passes. Release notes must separate “application-image audit passed” from “full Linux/boot validation passed”. Do not publish a normal compatibility release with a demonstrated unresolved startup failure; an unavailable optional hardware test should be reported as unperformed rather than fabricated.
