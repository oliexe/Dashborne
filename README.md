# Dashborne

An alternative UI for the S3XY DASH, built on its existing Linux OS.

Dashborne keeps the installed kernel, drivers and platform services while adding
a new interface and features. Development starts on microSD; removing the card
and restarting returns to the installed dashboard.

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

## Progress

MicroSD code execution is confirmed on a physical DASH. Our ARM library loaded
alongside the installed application and produced diagnostic results. The first
custom screen now builds with a touch counter and a button to open the original
dashboard. Local lifecycle and fallback tests pass; display and touch verification
on the DASH are next, followed by integration with the communication service.

- [Research](docs/research.md)
- [Roadmap](docs/roadmap.md)
- [SD probe: build and test](app/sd-probe/README.md)
- [Dashboard: build and test](app/dashboard/README.md)
