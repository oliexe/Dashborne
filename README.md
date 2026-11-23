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
`/bin/zkgui`; a FAT32 microSD card mounted at `/mnt/extsd` supplies `EasyUI.cfg`,
which selects Dashborne through `startupLibPath`. Our tested card uses an MBR
with an 8 GB FAT32 partition.

Dashborne shows a small centered icon and **Tap screen to enter configuration**.
Tap within three seconds to open configuration. Otherwise, it applies the
selected patches and opens the installed official dashboard.

Configuration provides Wi-Fi ADB with a live status/address, a selectable list
of patches installed on the card, and **Launch dashboard**. Settings are saved
on the card and remain selected on the next boot.

```text
EasyUI.cfg
Dashborne/lib/libdashborne.so
Dashborne/ui/loader.ftu
Dashborne/ui/icon.png
Dashborne/patches/catalog.tsv
Dashborne/patches/*.dbp
Dashborne/settings.conf
Dashborne/wifi.conf
```

The [Dashborne installer](app/dashborne/README.md) builds this card layout, saves
the previous startup configuration and selects the loader. It preserves display
and touch rotation, fonts and the installed `/res/ui/` resource root. Power off
before removing the card; booting without it starts the installed official app.
The patched resource is generated on the card and mounted temporarily, leaving
the installed resource unchanged.

See the [on-device findings](docs/research.md) and
[FlyThings SD startup documentation](https://developer.flythings.cn/zh-hans/start_from_sdcard.html).

## Progress

Development now focuses on patching the official app. The first injector applies
a version-matched, additive layout patch and forwards the original app callbacks.
Its initial patch adds a small Dashborne label; generated resources stay on the
card and override individual files through temporary mounts.
The unified Dashborne loader is installed as the card's startup app. Its splash
and automatic handoff to the official dashboard with the badge have been verified
on hardware. Earlier reboot tests restored the installed layout byte-for-byte.

The earlier Tesla-inspired [simulated cluster](app/dashboard/README.md) remains
available as a prototype. Its display and touch controls were verified on the DASH.

Wi-Fi ADB is verified on hardware, including a root shell, file transfers,
screen capture and remote UI updates in configuration. It currently becomes
unreachable after the official dashboard starts. Dashborne incorporates the
configuration and Wi-Fi ADB controls; the earlier ADB Enabler remains available
as a development fallback.

- [Dashborne: setup, configuration and patch selection](app/dashborne/README.md)
- [Official UI development copy](docs/official-ui.md)
- [Patch injector: build, preview and restore](app/injector/README.md)
- [Research](docs/research.md)
- [Roadmap](docs/roadmap.md)
- [Dashboard: build and test](app/dashboard/README.md)
- [ADB Enabler: setup and development loader](app/adb-enabler/README.md)
