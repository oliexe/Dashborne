# Patch injector

The [Dashborne startup app](../dashborne/README.md) now provides the splash,
configuration, patch selection and Wi-Fi ADB controls. This directory contains
the shared patch engine, patch definitions and the earlier standalone preview
tool used to validate resource overrides.

Dashborne starts the installed official app with small, separate patches. The
first patch adds a noninteractive turquoise **DASHBORNE** label at the bottom of
the dashboard. The original screens and controls remain in place.

## How it works

1. The SD `EasyUI.cfg` selects `libdashborne-injector.so`.
2. Before forwarding the official app initialization, the injector reads the
   installed `/res/ui/dash1act.ftu` and the 255-byte `badge.dbp` patch.
3. It checks the base file's length and CRC32, the FTU envelope and decompressed
   layout fingerprint, and the patch fragment checksum. These are compatibility
   and corruption checks, not package authentication.
4. It appends our control to the layout and writes the resulting FTU to the SD
   cache. A temporary bind mount overrides only `/res/ui/dash1act.ftu`.
5. The launcher forwards the official callbacks and startup activity unchanged.

The package consists of our loader, patch metadata and added control. The
modified layout is generated on the device from its installed app. Building the
patch needs no firmware download or base layout. Deployment additionally checks
the installed layout's SHA-256 before starting the preview.

The initial manifest targets the inspected **6.11.2 layout**. An unmatched layout
or missing patch is skipped and the original app starts. App updates need their
own inspected compatibility entry; the version label alone is insufficient.

## Reversibility

The installed resource file is never overwritten. The generated file lives at
`/mnt/extsd/Dashborne/injector/generated.ftu`, and its mount lasts only until
explicit restoration or reboot.

- **Power off, remove the microSD, boot:** the installed official app starts.
- **Reboot with a confirmed injector card:** the loader applies the patch again.
- **Restore over ADB:** the command below restores the previous SD selection and
  reboots, clearing the file mount and starting the previous app, normally ADB
  Enabler.
- **Skip patches on a card:** create `Dashborne/injector/disabled` while the DASH
  is powered off. The injector will forward the official app without applying a
  patch. Starting a new preview clears this marker.

Do not hot-remove the card to undo a running patch: the app has already read the
layout, and the mount still refers to the card. App preferences remain managed
by the official app under `/data`; reverting a UI patch does not reset them.

## Build and preview

```sh
DASHBORNE_CC=/opt/homebrew/opt/llvm/bin/clang \
DASHBORNE_LD=/opt/homebrew/opt/lld/bin/ld.lld \
sh app/injector/build.sh
sh app/injector/test.sh
python3 app/injector/deploy.py --device DEVICE_IP:5555
```

Preview deployment verifies uploaded files by reading them back. It saves the
current SD configuration, selects the injector for this session, and restores
the prior boot selection after startup. A two-minute recovery timer restores the
previous SD selection and reboots to clear the override and network state. A screenshot
is scheduled at 45 seconds in `Dashborne/injector/preview.png`.

After verifying the visible patch and connection, keep it as the card's startup:

```sh
python3 app/injector/deploy.py --device DEVICE_IP:5555 --keep
```

For a previously verified patch, `--activate` stages it from the helper and
selects it for microSD startup without the preview timer. Automatic startup has
not yet been verified for this injector; development currently uses previews.

To reboot without the override and return to the previous SD app:

```sh
python3 app/injector/deploy.py --device DEVICE_IP:5555 --restore
```

The launcher also attempts Wi-Fi ADB using `adb-enabler/wifi.conf`. Reliable ADB
connectivity alongside the official UI is still under investigation. The
recovery timer uses a reboot because restarting only the UI did not reliably
recover networking and the display during the first hardware preview.

## Validation

Local tests exercise the native patch engine with synthetic layouts, preserving
existing controls, rejecting mismatched versions and already patched files,
truncated/corrupted data, invalid lengths and extended FTU headers. A private
check against the installed layout confirms every original JSON field survives
and only our label is added. The ARM build and forwarding callback tests pass.
The device owner confirmed that the official dashboard rendered with the badge;
an on-device screenshot independently confirms it. The generated layout read
back from the device preserves every original JSON field and adds only the label.
After a physical reboot, the resource mount was absent and the installed layout
matched its original SHA-256 byte-for-byte. Restarting only the UI during the
first recovery attempt left the screen blank or frozen, so recovery now reboots.
The revised two-minute timer automatically rebooted into ADB Enabler, restored
Wi-Fi ADB, removed the mount and exposed the original byte-identical layout.
The helper remains the normal SD startup selection. Persistent injector startup
has not yet been tested; keeping ADB connected during the official UI remains
the next development issue.
