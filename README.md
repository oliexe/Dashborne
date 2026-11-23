# Dashborne

A microSD patch loader for the S3XY DASH's official app.

Dashborne loads the installed official UI and applies separate patches for visual
changes and extensions. The existing Linux, drivers and app remain the foundation.
Patches are applied from microSD; powering off, removing the card and booting
again returns to the installed dashboard. See the [patch injector](app/injector/README.md).

## Hardware

The S3XY DASH is an Enhance Ltd. instrument cluster, registered under
[FCC ID 2BUSZ-S3XYDASH](https://fccid.io/2BUSZ-S3XYDASH).

| Component | What we know | Source |
| --- | --- | --- |
| Processor | Allwinner T113 family; the submitted manual specifies a dual-core T113-S at 1.2 GHz. | [Internal photos, p. 2][photos]; [manual, p. 3][manual] |
| Display | 9.6-inch IPS touchscreen, 1920 × 480. | [Manual, p. 3][manual] |
| Wireless | Realtek RTL8733BS Wi-Fi/Bluetooth, plus a separate BLE module. | [Chipset declaration][chipsets]; [internal photos][photos] |
| Storage | Kingston-branded eMMC; capacity not yet established. | [Internal photos, p. 2][photos] |
| Power | USB-C. | [Enhance product FAQ](https://www.enhauto.com/pages/dash) |
| Expansion | microSD mounted at `/mnt/extsd`; application loading from FAT32 verified on our unit. | [On-device research](docs/research.md) |
| Software platform | Linux 5.4.61 with ARMv7 hard-float userspace and ZKSWE/FlyThings UI framework. | [On-device research](docs/research.md) |

[photos]: https://fccid.io/2BUSZ-S3XYDASH/Internal-Photos/Internal-photos-9104326.pdf
[manual]: https://fccid.io/2BUSZ-S3XYDASH/User-Manual/User-manual-9104334.pdf
[chipsets]: https://fccid.io/2BUSZ-S3XYDASH/Attestation-Statements/Kaspersky-Statement-9104348.pdf

## Loading from microSD

The DASH boots its installed Linux system from eMMC. Its UI service starts
`/bin/zkgui`, the FlyThings launcher. With a FAT32 microSD inserted, the card
mounts at `/mnt/extsd` and a card-root `EasyUI.cfg` selects the application library
through `startupLibPath`. This path has been verified on our DASH, including a
full reboot with ADB Enabler. See the [on-device findings](docs/research.md) and
[FlyThings SD startup documentation](https://developer.flythings.cn/zh-hans/start_from_sdcard.html).

Our tested card uses an MBR with an 8 GB FAT32 partition. For ADB Enabler, the
relevant files on that partition are:

```text
EasyUI.cfg
lib/libadb-enabler.so
ui/adb-enabler.ftu
adb-enabler/wifi.conf
```

In the card's existing `EasyUI.cfg`, set this field:

```json
"startupLibPath": "/mnt/extsd/lib/libadb-enabler.so"
```

Preserve the other configuration fields, including display/touch rotation,
fonts and `resPath: "/res/ui/"`. Our ARMv7 hard-float library forwards the installed
application's initialization callbacks and registers its own screen with the
existing framework. The kernel, drivers and platform services continue running.

The [ADB Enabler installer](app/adb-enabler/README.md) copies the files, saves the
previous launcher configuration and selects the helper. Insert the card with the
DASH powered off, then power it on. Removing the card while powered off and
starting again returns to the installed dashboard. Once ADB is connected, updated
app files and UI restarts can be handled remotely.

## Progress

Development now focuses on patching the official app. The first injector applies
a version-matched, additive layout patch and forwards the original app callbacks.
Its initial patch adds a small Dashborne label; generated resources stay on the
card and override individual files through temporary mounts.
The label has been verified on the official dashboard. Timed reboot recovery
restores ADB Enabler and leaves the installed layout byte-for-byte unchanged.

The earlier Tesla-inspired [simulated cluster](app/dashboard/README.md) remains
available as a prototype. Its display and touch controls were verified on the DASH.

Wi-Fi ADB is verified on hardware, including a root shell, file transfers,
screen capture and remote UI updates. A separate **ADB Enabler** helper provides
a minimal SD startup screen with automatic Wi-Fi ADB and a centered connection
address. Remote cluster previews preserve that helper as the startup selection.

- [Official UI development copy](docs/official-ui.md)
- [Patch injector: build, preview and restore](app/injector/README.md)
- [Research](docs/research.md)
- [Roadmap](docs/roadmap.md)
- [Dashboard: build and test](app/dashboard/README.md)
- [ADB Enabler: setup and development loader](app/adb-enabler/README.md)
