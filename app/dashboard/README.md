# Dashborne instrument cluster

A Tesla-inspired 1920 × 480 cluster running inside the DASH's installed
FlyThings UI process. The visual reference is the Model S cluster in
[this video](https://youtu.be/Ae1P49YXEgQ). The car, road, map and icons are
original vector artwork. All displayed vehicle and route values are dummy data.

The driving view combines animated lanes and traffic, a large speed readout,
speed limit, cruise setting, battery, range, temperature and gear indicators.
Speed cycles through 70–74 km/h; road animation advances at 10 frames per second.

| Tap | Action |
| --- | --- |
| Left panel | Switch between navigation and energy consumption |
| Center car/road | Toggle Park and Drive |
| Large speed readout | Pause or resume the simulation |

## Build and check

Requires Python 3.9+, [uv](https://docs.astral.sh/uv/), LLVM Clang and LLD with
ARM support. The build uses `uv` to run the pinned `resvg-py==0.5.0` renderer.
On macOS with Homebrew:

```sh
DASHBORNE_CC=/opt/homebrew/opt/llvm/bin/clang \
DASHBORNE_LD=/opt/homebrew/opt/lld/bin/ld.lld \
sh app/dashboard/build.sh
sh app/dashboard/test.sh
```

Outputs under this directory:

- `card/lib/libdashborne.so`: ARMv7 hard-float application adapter.
- `card/ui/dashborne.ftu`: compressed native layout.
- `card/ui/cluster/*.png`: generated background, panel and animation assets.
- `build/cluster-preview.png`: host-rendered composition preview.

Edit `screen.json` for control positions and typography, `assets.py` and
`assets/car.svg` for artwork, and `demo.c` for simulation behavior. Production
text uses the installed device fonts, so the host preview can differ slightly.
The build creates its link dependency stub locally; the running app loads
`/res/lib/libzkgui.so`.

Local tests cover the simulation, pause/Park behavior, native control callbacks,
lifecycle forwarding, compatibility fallbacks, layout bounds and SD staging.
The C tests run with address and undefined-behavior sanitizers. Rendering and
remote deployment have also been checked on the physical DASH.

## Preview over Wi-Fi ADB

Start with [ADB Enabler](../adb-enabler/README.md) running on the SD card and
connect the laptop to the address shown on its screen:

```sh
adb connect <displayed-ip>:5555
python3 app/dashboard/deploy.py --device <displayed-ip>:5555 \
  --wifi-config /path/to/wifi.json
```

The local JSON file contains `ssid` and `password` strings. Supply it on the first
cluster deployment; subsequent deployments can reuse `Dashborne/wifi.conf` on
the card. The cluster requests the configured Wi-Fi connection and leaves the
separate ADB daemon running. Debugging controls live in ADB Enabler.

The deployer uploads each file under a temporary name, reads it back to verify
its bytes, then renames it into place. It temporarily selects Dashborne and
restarts `zkswe`, waits for the cluster's startup log entry, then restores the
previous `EasyUI.cfg` without another restart. The cluster stays on screen;
rebooting returns to the previous startup app, normally ADB Enabler. Previous
library and layout files are retained with a `.previous` suffix.

Capture the actual screen:

```sh
adb -s <displayed-ip>:5555 shell '/res/bin/fbgrab -a -d /dev/fb0 /tmp/dashborne-screen.png'
adb -s <displayed-ip>:5555 pull /tmp/dashborne-screen.png screen.png
```

## Select the cluster at startup

To make the cluster the card's startup app instead of previewing it:

```sh
python3 app/dashboard/stage.py /Volumes/DASH_TEST --wifi-config /path/to/wifi.json
```

This preserves the previous launcher configuration and its other fields. Eject
the card, insert it with the DASH powered off, then power on. To restore the
previous selection after reconnecting the card to the laptop:

```sh
python3 app/dashboard/stage.py /Volumes/DASH_TEST --restore
```

## Runtime integration

The adapter registers `dashborneActivity`, forwards the installed application's
initialization and shutdown callbacks, and uses native controls and timers.
The simulation is separate from rendering so future vehicle data can replace
it without rebuilding the screen layout. PNG animation frames keep drawing
within the existing framework.

This adapter targets the inspected `/lib/libeasyui.so`: 826,052 bytes with
FNV-1a-32 `3ca7071f`. Missing bindings, a different runtime, an unreadable layout,
or a reported layout creation failure select the original startup screen.
A framework crash cannot be caught by this fallback.

Resource paths resolve to the SD card from the installed resource root. The
kernel, drivers and platform services continue running. Events append to
`/mnt/extsd/Dashborne/events.log`, including `cluster-created: simulated-data`,
`dashborne-selected`, and `demo:` interaction entries.
