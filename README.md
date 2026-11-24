# Dashborne

A microSD boot menu and temporary extension loader for the S3XY DASH's official UI.

## Install

After building Dashborne, install it from the repository root:

1. Prepare a microSD card with an **MBR partition table and a FAT32 partition**.
   We tested an 8 GB FAT32 partition. Formatting erases the card.
2. Mount the card and run:
   ```sh
   python3 dashborne/install.py --card /Volumes/CARD
   ```
   Replace `/Volumes/CARD` with its mount path. The installer copies
   **`dashborne/build/card/Dashborne/`** to the card root and creates **`EasyUI.cfg`**
   and **`dashborne.conf`**. Upgrades keep your settings and startup backup.
3. Eject the card, insert it into the powered-off DASH, then power on.
   **Tap the splash within three seconds** to configure Dashborne; otherwise it
   continues to the official dashboard with your selected tweaks.

For developer Wi-Fi, edit `wifi.ssid` and `wifi.password` in the card-root
`dashborne.conf`, then enable ADB in the boot menu. ADB is off by default.
Power off and remove the card to boot the original dashboard without Dashborne.

## Boot menu

- Patches: enable individual tweaks and extensions.
- ADB: opt-in Wi-Fi debugging with connection status and address.
- Loader: browse and launch compatible apps from microSD, with optional autolaunch.
- Device: software versions, serials and system information.

Dashborne saves preferences in the card-root `dashborne.conf`.
See [configuration and operation](dashborne/README.md).

## Extensions

- [Extra menu](extensions/extra-menu): the core star button opens a stock-style
  **Tweaks** menu where extensions can register pages.
- [Contrast](extensions/contrast): display contrast and luminance controls in stock Settings.
- [Screen Mirroring](extensions/screen-mirroring): a dedicated iPhone mirroring
  window with aspect controls; Back restores CarPlay/Android Auto.
- [Dashborne badge](dashborne/patches/definitions/badge.json): an optional dashboard label.

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

## Development

```text
dashborne/             Boot menu, configuration, ADB, Loader and Device
  extensions/          Native extension engine and SDK
  patches/             Layout patch engine, compiler and definitions
extensions/            Bundled extensions
docs/                  Platform and SD loading research
```

Build with Python 3.10+, Clang and LLD; host tests also need a C compiler and zlib.
From the repository root:

```sh
sh dashborne/build.sh
sh test.sh
```

For Homebrew LLVM/LLD on macOS:

```sh
DASHBORNE_CC=/opt/homebrew/opt/llvm/bin/clang \
DASHBORNE_LD=/opt/homebrew/opt/lld/bin/ld.lld \
sh dashborne/build.sh
```

The build packages Dashborne and the bundled extensions into `dashborne/build/card/`.
See the [extension SDK](dashborne/extensions/README.md),
[layout patches](dashborne/patches/README.md), and [platform research](docs/research.md).
