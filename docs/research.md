# Research record

## Confirmed on the test DASH — 2026-11-23

- Linux 5.4.61 is already installed. Userspace includes ARMv7 hard-float binaries.
- The application platform is ZKSWE/FlyThings. The firmware target and SDK paths
  identify a T113 eMMC platform. This does not establish the assembly factory.
- A 512 GB microSD with an MBR and an 8 GB FAT32 partition mounted at `/mnt/extsd`.
- A card-root `EasyUI.cfg` selecting our library through `startupLibPath` caused
  its constructor to execute. A generated success marker and process memory map
  independently confirmed the result.
- The initial test library declared the installed `/res/lib/libzkgui.so` as a
  runtime dependency. Both libraries were mapped in the existing UI process.
- This worked with `zkdebug` set to false in the card configuration.
- `/res` was read-only ext4, `/late` the read-only configuration partition, and
  `/data` writable ext4. Startup configuration references application and backup
  partitions through `abprobe`.
- Startup configuration declares the UI service as running as root. Effective
  privileges at our entry point have not been checked directly.
- A disabled ADB service is defined. USB ADB was not exposed to the development
  laptop; the cable, USB role and gadget configuration remain separate questions.

## Architecture understood so far

The normal Linux startup launches the FlyThings loader, which selects an
application library. Our SD library can participate in this existing process.
This proves an application extension path; it does not prove SD kernel boot,
full UI replacement, or successful completion of every original UI callback.

The original UI library also performs initialization and update orchestration.
The `enhcom` process handles Enhance communication and exposes local control/data
sockets. A replacement interface must preserve or deliberately take over required
initialization. Two UIs must not blindly compete for display or touch ownership.

The downloaded display update contains the application partition, not a complete
OS image. Kernel/bootloader replacement is unnecessary for the project's goal.

## Open questions

- Can the app activate USB ADB and establish a shell from the laptop?
- Does forwarding the original callbacks preserve every required service lifecycle?
- What local IPC is available for reading vehicle/device data?
- How should new UI code coexist with pairing, connectivity and power management?
- Does the installed launcher impose watchdog or fallback requirements?

## Launcher and first screen — static analysis

The loader resolves `onEasyUIInit(context)`, `onStartupApp(context)` and
`onEasyUIDeinit(context)`. The startup callback returns an activity name.
These contracts agree with the
[FlyThings project documentation](https://docs.flythings.cn/zh-hans/project_structure.html).
The inspected application initialization starts logging, `enhcom`, and its widget
loader. Dashborne forwards that initialization instead of recreating it.

The captured framework exports an activity factory and base activity methods.
Its resource loader reads a compressed JSON layout inside an FTU envelope.
Our first screen uses those existing controls and event dispatch, with original
layout content and a small C adapter for the inspected ARM ABI. The adapter checks
the runtime fingerprint and relevant vtable entries before registering the screen.

The first screen's rendering and touch input were subsequently confirmed by the
user on the physical DASH. Full shutdown and service coexistence testing remain.

## USB debugging

The captured init configuration defines a disabled `/bin/adbd` service. A
`sys.usb.config=adb` action configures the gadget at
`/sys/class/zkswe_usb/zkswe0`, with USB ID `18d1:d002`. Separate persistent-debug
triggers start or stop the daemon. The app instead requests the service through
temporary `ctl.start`/`ctl.stop` properties, matching the
[Android init service-control mechanism](https://android.googlesource.com/platform/packages/modules/adb/+/refs/heads/main/docs/dev/how_adbd_starts.md).

The inspected runtime's `SystemProperties` string methods directly forward to
`property_get` and `property_set`. The debug switch uses these methods, selects
USB transport, and polls the daemon state and gadget enable node. A running daemon
and enabled gadget do not establish that the laptop has connected. USB enumeration
and an ADB shell still need to be tested on this device.

The first on-device attempt reported the legacy gadget control unavailable;
the event log confirms the app skipped the ADB start request. This check groups
open/read failures and unexpected contents, so it cannot establish that the port
lacks data wiring.

The downloaded 6.11.4 application package includes a `setusbconfig` helper that
uses ConfigFS, FunctionFS and `/sys/class/udc`, rather than the legacy path in the
captured init script. Its presence is evidence of another supported software path,
not proof that a controller is active or connected to the populated connector on
this unit. The new read-only snapshot checks the running system before selecting
a backend. See the kernel's
[ConfigFS gadget documentation](https://docs.kernel.org/usb/gadget_configfs.html)
for controller binding and gadget setup.

The next on-device report confirmed `/sys/class/udc/4100000.udc-controller`,
`/res/bin/setusbconfig`, and `/bin/adbd`. Thus the running kernel exposes a USB
device controller. The diagnostic script's other results were incomplete because
utilities including `printf`, `tr`, and `head` were unavailable. Its replacement
uses shell builtins and the observed `ls` command.

Dashborne now includes a native ConfigFS backend for that controller. It creates
its own gadget, prepares FunctionFS, starts the installed ADB daemon, and waits
for its endpoints before binding. The endpoint paths follow
[AOSP's ADB definitions](https://android.googlesource.com/platform/packages/modules/adb/+/refs/heads/main/adb.h).
Local tests cover setup, delayed endpoints, binding failures, unbinding and repeat
enable. Hardware enumeration remains unverified. An exposed controller does not
establish which board connector, if any, has its data pins routed.

On the next physical test, the UI reported the gadget bound. In this build that
requires the ADB daemon to report running, its FunctionFS endpoints to exist,
and the controller-binding write to succeed. The Mac nevertheless showed neither
the expected `18d1:d002` USB device nor an ADB transport. The original status label
grouped several kernel states as “enumerating”; it did not prove host enumeration.
The revised code distinguishes those states and logs the raw kernel value.

## Wi-Fi debugging

Physical testing confirmed the configured station connection and network ADB on
port 5555. The laptop listed the DASH as an active ADB device, and a shell reported
UID/GID 0 on Linux 5.4.61. Reading the SD event log confirmed that Dashborne requested
the connection and started the TCP daemon. A small file pushed to `/tmp` and pulled
back matched byte-for-byte; the temporary file was then removed. This establishes
working shell and file-transfer access without moving the microSD card.

The first connection attempts failed from the laptop's existing ADB server even
though ping and a direct TCP connection succeeded. Restarting that server restored
connectivity. USB enumeration remains a separate unresolved issue.

Screen capture through the installed `/res/bin/fbgrab` utility produced the live
Dashborne UI. Restarting the `zkswe` init service changed the UI process while the
separate ADB service stayed available. A subsequent remote deployment uploaded a
new Dashborne library, verified its bytes by downloading it again, retained the
previous library, installed the new file by rename, and restarted the UI. The
captured screen confirmed the changed ADB status display. This verifies the
build/upload/restart/capture development cycle without removing the card.

## Public references

- [FlyThings: launch from microSD](https://developer.flythings.cn/zh-hans/start_from_sdcard.html).
  Documents FAT32 and a card-root application configuration; verified on our unit
  by the initial load experiment and the ADB Enabler reboot test.
- [ZKSWE/FlyThings](https://github.com/zkswe): platform vendor.
- [Independent ZKSWE format research](https://github.com/atomicstack/tc002-customisation):
  useful background on another device. Its hardware details, partition sizes and
  recovery behavior are not evidence about the DASH.
