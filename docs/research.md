# Research record

## Confirmed on the test DASH — 2026-11-23

- Linux 5.4.61 is already installed. Userspace includes ARMv7 hard-float binaries.
- The application platform is ZKSWE/FlyThings. The firmware target and SDK paths
  identify a T113 eMMC platform. This does not establish the assembly factory.
- A 512 GB microSD with an MBR and an 8 GB FAT32 partition mounted at `/mnt/extsd`.
- A card-root `EasyUI.cfg` selecting our library through `startupLibPath` caused
  its constructor to execute. A generated success marker and process memory map
  independently confirmed the result.
- The probe declared the installed `/res/lib/libzkgui.so` as a runtime dependency.
  Both libraries were mapped. The probe did not replace or flash that application.
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

- Does the custom activity render and handle touch correctly on the DASH?
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

The screen is implemented and locally tested with a simulated runtime. Its
registration, rendering and coexistence with the original UI are not yet confirmed
on hardware; only the earlier SD probe has run on the device.

## Public references

- [FlyThings: launch from microSD](https://developer.flythings.cn/zh-hans/start_from_sdcard.html).
  Documents FAT32 and a card-root application configuration; verified on our unit
  by the probe described above.
- [ZKSWE/FlyThings](https://github.com/zkswe): platform vendor.
- [Independent ZKSWE format research](https://github.com/atomicstack/tc002-customisation):
  useful background on another device. Its hardware details, partition sizes and
  recovery behavior are not evidence about the DASH.
