# Dashborne

Dashborne is the microSD startup app, configuration screen and temporary patch
loader for the official S3XY DASH UI.

## Startup

The centered icon is followed by **Tap screen to enter configuration**. The
countdown is three seconds. Tapping anywhere on the splash cancels it and opens
configuration; there is no further timeout while configuring. Otherwise,
Dashborne applies the selected patches and opens the official startup activity
from the installed app in flash.

Configuration includes:

- **Wi-Fi ADB:** developer opt-in, off by default; enable/disable TCP ADB
  and display connection status/address in configuration.
- **Patches on microSD:** check individual compatible patches. Four are shown
  per page, with navigation for a catalog of up to 16.
- **Launch dashboard:** save selections, apply the patches, open the official UI.

Selections are written to `Dashborne/settings.conf`. Network credentials are
loaded from `Dashborne/wifi.conf`, with an SSID line followed by a passphrase
line. The installer can read a local JSON file containing `ssid` and `password`,
or migrate the existing ADB Enabler Wi-Fi file. New settings always default ADB
off, including migrations from ADB Enabler. Reinstalling preserves an explicit
Dashborne selection.
The badge patch is initially selected when compatible.

## Build and install

```sh
DASHBORNE_CC=/opt/homebrew/opt/llvm/bin/clang \
DASHBORNE_LD=/opt/homebrew/opt/lld/bin/ld.lld \
sh app/dashborne/build.sh
sh app/dashborne/test.sh
python3 app/dashborne/install.py --device DEVICE_IP:5555
```

For a mounted card with its existing `EasyUI.cfg`:

```sh
python3 app/dashborne/install.py --card /Volumes/CARD --wifi-config /path/to/wifi.json
```

The installer sets `startupLibPath` to
`/mnt/extsd/Dashborne/lib/libdashborne.so`, preserves the other configuration
fields, and saves the previous selection in `Dashborne/previous.cfg`. Reinstalling
preserves saved patch choices and Wi-Fi settings. Uploads are read back and
compared before activation.

Use `--configure` to open configuration once, bypassing the splash. A manually
created `Dashborne/config.once` file has the same effect and is consumed on start.
Development previews use `--preview` with an automatic four-minute reboot to the
previous startup selection; `--keep` retains the preview as the normal startup.

## Patch files

Patch definitions live in [`app/injector/patches`](../injector/patches). The build
compiles each definition to a small `.dbp` and generates `patches/catalog.tsv`.
Copy the compiled files and catalog together to `Dashborne/patches` on the card.
The loader discovers entries from that catalog on startup.

Each entry has a stable ID, title, description, installed-layout fingerprint and
an additive control definition. The build rejects duplicate IDs/control IDs and
keys. The initial patch engine supports additive controls in `dash1act.ftu`;
additional kinds of behavior patches will require further engine support.

The loader checks each patch against the installed layout. Incompatible or
missing patches cannot be selected. Selected compatible fragments are combined,
validated again, and applied to the device's own layout. The result is generated
as `Dashborne/generated.ftu` and bind-mounted over the original resource. An
application failure remains in configuration with an error rather than launching
with a partially built patch set. Selecting no patches opens the official UI
without a resource override.

## Reversal

Power off, remove the microSD, then boot to return to the installed official app.
A reboot clears temporary mounts; with Dashborne's card inserted, the selected
patches are applied again. The loader also clears its previous layout override
when reloaded so configuration starts from the installed base resource.

To restore the SD startup selection that preceded Dashborne and reboot:

```sh
python3 app/dashborne/install.py --device DEVICE_IP:5555 --restore
```

The same `--restore` option works on a mounted card while the DASH is off. Official
app preferences under `/data` are managed by the vendor app; restoring the loader
selection does not reset those preferences.

## Development checks

The native callback tests cover automatic launch, tap-to-cancel, checkbox actions,
save/apply failures, runtime compatibility and cleanup. Settings tests cover
round-trips, malformed catalogs, missing patches, and composing independent patches
while preserving every original layout field. Installer tests cover Wi-Fi
migration, preserving choices, and restoring the exact previous configuration.

The configuration screen renders on hardware, including the supplied icon and
live Wi-Fi ADB status. The splash automatically opens the official dashboard with
the selected badge patch, confirmed on the DASH. The loader is installed as the
card's startup selection. After a full reboot, tapping the splash opens
configuration and cancels automatic launch, also confirmed on the touchscreen.
Rebooting into configuration clears the patch: the installed layout matches its
original SHA-256 byte-for-byte.

Developer Wi-Fi connections are managed only while configuration is open.
The splash never requests station mode, even with ADB selected. Handoff stops
the network timer; Dashborne makes no Wi-Fi reconnect or ADB restart requests
while the official UI runs. Its CarPlay/Android Auto hotspot remains under the
official app's control. An existing ADB connection may drop when that app changes
network mode; debugging through the hotspot is not yet implemented.
