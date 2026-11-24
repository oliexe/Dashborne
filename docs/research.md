# Platform research

On-device findings from the test DASH. Hardware specifications and public
sources are in the [main README](../README.md#hardware).

## Loading from microSD

The DASH boots its installed Linux system from eMMC. The UI service starts
`/bin/zkgui`; a card-root `EasyUI.cfg` selects the application shared library
through `startupLibPath`. A 512 GB microSD with an MBR and an 8 GB FAT32 partition
mounted at `/mnt/extsd` and loaded Dashborne on the installed OS. We have not
established whether the DASH can boot a kernel from SD.

Dashborne selects `/mnt/extsd/Dashborne/lib/libdashborne.so`, retains `/res/ui/`
as the runtime resource root, and depends on the installed
`/res/lib/libzkgui.so`. The startup library exports `onEasyUIInit(context)`,
`onStartupApp(context)` and `onEasyUIDeinit(context)`. The startup callback
returns an activity name. These contracts agree with the [FlyThings project
documentation](https://docs.flythings.cn/zh-hans/project_structure.html).

On the device, we confirmed the three-second splash, tap-to-configuration,
display/touch controls and handoff to the patched official dashboard. Loading does not
require `zkdebug` to be enabled. Powering off and removing the card returns to the
installed dashboard. See [installation](../README.md#install).

## Runtime and resources

- Linux 5.4.61, ARMv7 hard-float userspace and ZKSWE/FlyThings UI framework.
- Firmware target and SDK paths identify a T113 eMMC platform.
- `/res` and `/late` were read-only; `/data` was writable ext4.
- The UI service is configured as root; a Wi-Fi ADB shell confirmed UID/GID 0.
- The official app initializes logging, `enhcom` communication and its widget
  loader. Dashborne forwards its callbacks to preserve that initialization.
- Layout resources contain compressed JSON inside an FTU envelope. The runtime
  supplies activity factories, controls and event dispatch. Native adapters check
  runtime fingerprints and relevant ABI details before use.

## Temporary extensions

Dashborne checks compatibility against the installed resource fingerprint before
applying layout patches.

Layout patches generate a modified resource on microSD and bind-mount it over
the original for the session. After rebooting, we checked that the installed
layout matched its original bytes. The native engine provides lifecycle
callbacks, symbol access, custom activities and journaled read-only overlays.
Hardware tests verified ARM extension loading, overlay cleanup and stale-mount
recovery without changing the original resource. See the [extension
SDK](../dashborne/extensions/README.md).

## Developer access

Through Wi-Fi ADB on TCP 5555, we obtained a root shell, compared uploaded and
downloaded files, captured the framebuffer with `/res/bin/fbgrab`, and deployed
UI updates by restarting `zkswe`. Disabling and re-enabling ADB in configuration
restored the connection. The tested Wi-Fi driver lacks some additional wireless
ioctls, so the status can report limited Wi-Fi verification.

Dashborne manages developer Wi-Fi only while configuration is open. The official
app uses hotspot mode for CarPlay/Android Auto; starting it can disconnect the
station-mode ADB session. Dashborne stops reconnect attempts at handoff and leaves
network ownership to the official app.

USB tests found `/sys/class/udc/4100000.udc-controller`, `/res/bin/setusbconfig`
and `/bin/adbd`. A ConfigFS/FunctionFS gadget reached the bound state, but the
Mac did not enumerate the expected ADB device. We have not confirmed data-pin
routing to the populated connector or a working USB debugging connection.

## References

- [FlyThings: launch from microSD](https://developer.flythings.cn/zh-hans/start_from_sdcard.html)
- [ZKSWE/FlyThings platform vendor](https://github.com/zkswe)
- [Linux ConfigFS USB gadgets](https://docs.kernel.org/usb/gadget_configfs.html)
- [Independent ZKSWE format research](https://github.com/atomicstack/tc002-customisation)
  documents the format on another device. Its hardware findings do not establish
  DASH specifications.
