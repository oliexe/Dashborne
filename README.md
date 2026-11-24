> [!IMPORTANT]
> Dashborne is a community-driven hobby project, not affiliated with Enhance Ltd. or Tesla, Inc.
> No vendor firmware or proprietary libraries are bundled.
> Provided **as is**, without warranty—use at your own risk.

<img src="dashborne/assets/icon.png" alt="Dashborne logo" width="160">

# Dashborne

Dashborne is an ADB enabler, extension loader, and app loader for the S3XY DASH's
official Linux-based OS. It gives users the tools to modify every aspect of the
DASH's software or load custom apps. It loads extensions from a microSD card and
applies them temporarily, without writing them to the DASH's internal flash memory.

## FAQ

<details>
<summary>What does it allow?</summary>

You can change the DASH's UI and behavior, add features through extensions,
load custom apps, and enable ADB for development.

</details>

<details>
<summary>Can it brick my device?</summary>

Dashborne's normal extension loading leaves the OS in flash untouched.
Custom apps and extensions run with system privileges, so code that writes to
internal storage can still damage the system.

</details>

<details>
<summary>Are these extensions permanent?</summary>

No; Dashborne's normal extensions apply only while running from the card.
Power off and remove the microSD card to boot the unpatched OS.

</details>

<details>
<summary>What if an extension fails to load?</summary>

If an extension reports a loading or activation error, Dashborne stays in the boot
menu and shows the error so you can disable it.
If it crashes or freezes the UI, power-cycle the DASH and tap the splash to
disable the extension, or remove the card to boot stock.

</details>

<details>
<summary>Can I create my own apps and extensions?</summary>

Yes, you can build custom apps and extensions for the DASH's Linux platform.
Use the [extension SDK](dashborne/extensions/README.md) for extensions and the
[Loader](dashborne/README.md) to launch compatible apps from microSD.

</details>

<details>
<summary>Can S3XY DASH run DOOM?</summary>

Yes.

</details>

## Install

Download the microSD ZIP from the [latest release](https://github.com/oliexe/Dashborne/releases/latest)
and extract it.

1. Prepare a microSD card with an **MBR partition table and a FAT32 partition**.
   We tested an 8 GB FAT32 partition. Formatting erases the card.
2. On a fresh card, copy everything inside the ZIP's `card/` directory to the
   card root: **`Dashborne/`**, **`EasyUI.cfg`** and **`dashborne.conf`**.
   For an upgrade, run this from the extracted ZIP using Python 3.10+:
   ```sh
   python3 install.py --card /Volumes/CARD
   ```
   Replace `/Volumes/CARD` with its mount path. The installer preserves your
   settings and startup backup.
3. Eject the card, insert it into the powered-off DASH, then power on.
   **Tap the splash within three seconds** to configure Dashborne; otherwise it
   continues to the official dashboard with your selected extensions.

For developer Wi-Fi, edit `wifi.ssid` and `wifi.password` in the card-root
`dashborne.conf`, then enable ADB in the boot menu. ADB is off by default.
Power off and remove the card to boot the original dashboard without Dashborne.

## Boot menu

With a Dashborne microSD card inserted, the S3XY DASH shows the Dashborne boot
screen at startup. **Tap the screen within three seconds** to enter the boot menu;
otherwise, Dashborne continues to the official dashboard with your selected extensions.

![Dashborne boot menu on the S3XY DASH, showing the boot options](docs/images/boot-menu.png)

- Extensions: enable or disable extensions on your microSD card.
- ADB: enable root access over Wi-Fi, useful for development and modding the device.
- Loader: browse and launch compatible apps from microSD, with optional autolaunch.
- Device: view hardware details, software versions, serials and system information.

Dashborne saves preferences in the card-root `dashborne.conf`.
See [configuration and operation](dashborne/README.md).

## Extensions

Custom extensions can modify any part of the DASH's Linux-based OS, including
its UI, features, and behavior. Dashborne includes the
[Extra menu](dashborne/core/extra-menu) and
[widget runtime](dashborne/core/widget-runtime), which extensions can use to add
entries to the floating Extra menu or build custom widgets for the stock widget picker.

- [Widget Example](extensions/widget-example): boilerplate for adding widgets to any stock picker category.
- [Screen Mirroring](extensions/screen-mirroring): AirPlay and experimental Miracast in a shared
  viewer with aspect controls; Back restores CarPlay/Android Auto.

See the [extension development guide](dashborne/extensions/README.md) to build your own.

## ADB Debugging

Dashborne includes a Wi-Fi ADB enabler that gives you root access to the DASH's
Linux system. You can read firmware and system files, make changes on the fly,
debug applications, and reverse-engineer the device's software.

Refer to the [boot menu](#boot-menu) section to enable the ADB bridge.

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
Install a source build with `python3 dashborne/install.py --card /Volumes/CARD`,
or create a downloadable ZIP with `python3 tools/release.py`.

The [build workflow](.github/workflows/release.yml) builds and tests pushes to
`main`, pull requests and manual runs. To publish a release, update `VERSION` and
the core [changelog](CHANGELOG.md), commit them, then push a matching `vX.Y.Z` tag.
The workflow publishes the ZIP and its SHA-256 checksum after the tests pass.

See the [extension SDK](dashborne/extensions/README.md),
[layout patches](dashborne/patches/README.md), and [platform research](docs/research.md).

# Device Exploration

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

Other aftermarket Tesla clusters have similar specifications:

| Product | Published similarities |
| --- | --- |
| [Yeslak T9](https://www.yeslak.com/products/9-6-carplay-instrument-cluster-for-tesla-model-3-y-ly) | T113-S dual-core 1.2 GHz, Linux, 9.6-inch 1920 × 480 IPS display, 258 × 75 × 15 mm and 323 g—all match the DASH's submitted [manual, p. 3][manual]. |
| [TESLASY T9 Gen 2](https://www.teslasy.com/products/model-3-y-model-3-highland-t9-9-6-inch-gen-2-carplay-instrument-cluster-head-up-touch-display) | 9.6-inch Linux cluster with CarPlay, Android Auto, OTA updates and magnetic mounting; the listing does not identify its processor. |
| [TESERY 9.6-inch cluster](https://www.tesery.com/products/9-6-inch-instrument-cluster-dashboard-display-for-tesla-model-3-y) | Linux, 1920 × 480 IPS display, CarPlay and Android Auto; the listing does not identify its processor. |

Our working hypothesis is that the S3XY DASH is a slightly modified and rebranded
[Yeslak T9](https://www.yeslak.com/products/9-6-carplay-instrument-cluster-for-tesla-model-3-y-ly)
or a similar device.

Enhance's submitted [manual][manual] reproduces the 24-page
[T9 manual linked by Yeslak](https://drive.google.com/file/d/1kAx3nM93lCkuczWnp6HKOUSXhaLoSg5Z/view),
including its specifications, wiring diagrams, UI illustrations and installation
photos, with "S3XY Dash" added to the cover and an extra FCC compliance page.

## Software

The DASH runs Linux 5.4.61 with ARMv7 hard-float userspace and the
ZKSWE/FlyThings UI framework. See the [on-device research](docs/research.md).

[photos]: https://fccid.io/2BUSZ-S3XYDASH/Internal-Photos/Internal-photos-9104326.pdf
[manual]: https://fccid.io/2BUSZ-S3XYDASH/User-Manual/User-manual-9104334.pdf
[chipsets]: https://fccid.io/2BUSZ-S3XYDASH/Attestation-Statements/Kaspersky-Statement-9104348.pdf
