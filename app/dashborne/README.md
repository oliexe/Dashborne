# Dashborne

Dashborne is the microSD startup app, configuration screen and temporary patch
loader for the official S3XY DASH UI.

## Startup

The centered icon is followed by **Tap screen to enter configuration**. The
countdown is three seconds. Tapping anywhere on the splash cancels it and opens
configuration; there is no further timeout while configuring. Otherwise,
Dashborne applies the selected patches and opens the official startup activity
from the installed app in flash.

Configuration uses a black background with a category sidebar on the left and
the selected content on the right. **Patches** opens by default; **ADB** contains
the developer connection controls. Switching sections preserves selections and
the list position. The footer places the development version from `VERSION` and the repository
URL beneath the Dashborne branding, leaving room for five patch rows at a time.

Configuration includes:

- **ADB / Enable ADB:** developer opt-in, off by default; enable/disable TCP
  ADB. The page uses aligned rows for the switch, live status, IP address with
  port, and configured developer Wi-Fi name. Address and network fields clear
  when ADB is disabled.
- **Patches:** swipe the native list to browse up to 64 entries. Tap a row to
  toggle its patch; each row has a title, description and switch. Incompatible
  patches have a disabled switch. Selections save immediately, and the header
  shows how many patches are enabled.
- **Loader:** browse microSD folders, select an app, and tap **Load app**.
  **Up** navigates to the parent folder and **Refresh** rescans the current folder.
- **Continue booting:** save selections, apply the patches, open the official UI.

Selections are written to `Dashborne/settings.conf`. Network credentials are
loaded from `Dashborne/wifi.conf`, with an SSID line followed by a passphrase
line. The installer can read a local JSON file containing `ssid` and `password`,
or migrate the existing ADB Enabler Wi-Fi file. New settings always default ADB
off, including migrations from ADB Enabler. Reinstalling preserves an explicit
Dashborne selection.
The badge patch is initially selected when compatible.

## Load an app from microSD

Open **Loader**, enter the app's folder, tap its `.so` file, then tap **Load app**.
Directories appear first. Selecting a file does not execute it. Unsupported files
remain visible, but cannot be launched. The browser stays within the microSD root
and shows up to 256 entries per folder; larger folders display a limit message.

This launches native DASH/FlyThings apps: ARMv7 Linux hard-float shared libraries
exporting `onEasyUIInit`, `onStartupApp`, and `onEasyUIDeinit`. A Dashborne extension
is a different interface and belongs in **Patches**. Standalone Linux executables,
shell scripts, and Android APKs are not supported by this launcher.

Keep the app's resources and dependencies on the card in the paths expected by
that app. The runtime resource root remains `/res/ui/`; apps can reference their
own SD layouts with `../../../mnt/extsd/...`, as the dashboard example does.
Loading an app does not apply the selected dashboard patches.

The loader validates the selected ELF, then writes a one-use request in `/tmp`
and restarts only the UI service. The new process consumes the request **before**
loading the library, delegates initialization/startup/shutdown to that app, and
keeps its module resident. A failed load returns to Loader. If the UI service
restarts after the app exits or crashes, the consumed request brings it back to
Dashborne configuration instead of selecting the app again. A hung app still
requires a restart or power cycle.

`EasyUI.cfg`, the installed OS, and saved patch/ADB choices stay unchanged.
Power cycling clears the temporary selection and resumes normal Dashborne boot.

## ADB connection handling

Enable ADB manages a developer connection only inside configuration. The controller
confirms the previous TCP daemon has stopped, validates `wifi.conf`, requests station
mode when needed, waits for association, then starts ADB. A healthy existing
connection to the configured SSID can be reused. The toggle remains your saved
choice; the status below it reports the actual progress or failure.

An address is shown only when the station interface is up, the vendor reports a
connection, the driver association/SSID checks pass, and the daemon has a TCP 5555
listener on that address or a wildcard address. Drivers that explicitly do not
support the additional wireless checks show **limited Wi-Fi verification**.
**ADB listening** verifies the device's local state, not reachability from the
Mac: router isolation, routing and driver faults can still block the connection.

The controller clears stale displayed addresses, retries failed Wi-Fi connections
and missing ADB listeners up to three times, and reports a failure rather than
retrying forever. Toggle off/on to retry after correcting the cause. Disabling
cancels connection retries, stops TCP ADB, and reports **Disabled** only after the
service confirms it stopped. Stop failures remain visible. An ADB daemon using a
different transport is left alone and identified in the status.

If saving an enabled selection fails, ADB stays off. A disable request takes
effect even if saving fails, with the card-write error shown. Pending work is
cancelled on dashboard handoff or loader shutdown; the official app then owns
networking. No further reconnect or daemon restart requests are issued by Dashborne.

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

Entries can be versioned layout patches or [native extensions](../extensions/README.md).
Layout patches add controls or edit guarded scalar properties in `dash1act.ftu`.
Native extensions can add features, register custom activities, hook existing
behavior, and override theme resources. They activate only when launching the
dashboard; installing one leaves it disabled until selected. The build rejects
duplicate IDs and additive control IDs/keys.

The loader checks each patch against the installed layout. Incompatible or
missing patches cannot be selected. Selected compatible layout edits are combined,
validated again for conflicts, and applied to the device's own layout. The result is generated
as `Dashborne/generated.ftu` and bind-mounted over the original resource. An
application failure remains in configuration with an error rather than launching
with a partially built patch set. Selecting no layout patches skips that layout override. Enabled native extensions
then activate as a group; a failure rolls them back and remains in configuration.
The engine tracks its resource mounts for cleanup and recovery after UI restarts.

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

The ADB controller tests inject delayed and failed service operations, rapid
switch changes, a queued start cancelled by disable, missing APIs/configuration,
short and failed reads, invalid credentials, wrong SSIDs, stale/invalid addresses,
lost links, unsupported wireless checks, and malformed listener records. They
exercise cancellation at each connection stage and 10,000 deterministic mixed
fault/toggle events. Native tests run with AddressSanitizer and UndefinedBehaviorSanitizer.
The deployed library was read back over Wi-Fi ADB and matched the build's SHA-256.
A framebuffer capture confirms the sidebar and ADB controls render correctly.
This DASH reports **limited Wi-Fi verification**: its driver does not support at
least one of the additional wireless ioctls. The controller's fallback works on
hardware, and the Mac can establish an ADB session and transfer files. Seven
successful shell checks across one minute found no controller transitions or
changes to the running TCP service state. A physical off/on cycle also passed:
the UI and log reported Disabled, TCP 5555 became unreachable, and enabling ADB
restored the listener and a working Mac shell session. Repeated cycles and
network-failure recovery remain to be checked on hardware.

The native callback tests cover automatic launch, tap-to-cancel, switch actions,
category navigation, list adapter callbacks and selecting entries beyond the
first visible rows,
save/apply failures, runtime compatibility and cleanup. Settings tests cover
round-trips, malformed catalogs, missing patches, and composing independent patches
while preserving every original layout field, plus a full 64-entry settings
round-trip. Installer tests cover Wi-Fi migration, preserving choices, and restoring the exact previous configuration.

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

The patch browser uses the runtime's [native list control](https://developer.flythings.cn/zh-hans/listview.html).
Only visible rows are populated through the adapter; scrolling and drag-versus-tap
handling remain with the framework. Switch images are original geometric assets
rendered by `switches.py` during the build.
