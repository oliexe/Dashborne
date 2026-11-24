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
- **Device:** a scrollable table of software versions, device identifiers, board
  details, memory, uptime, display mode and storage serials/capacities. Values are
  read when the page opens; **Refresh** updates the snapshot. Missing values show
  **Unavailable**, and unrecognized dashboard binaries are identified as such.
- **Continue booting:** save selections, apply the patches, open the official UI.

## Configuration file

All persistent Dashborne settings live in **`/mnt/extsd/dashborne.conf`**, at the
microSD root. The boot menu and installer use this same file.

```ini
format=1
adb.enabled=0
wifi.ssid=Your Wi-Fi name
wifi.password=Your Wi-Fi password
tweak.dashborne-badge.enabled=1
tweak.contrast.enabled=0
tweak.screen-mirroring.enabled=0
loader.autolaunch=0
loader.app=
```

Keys are namespaced and booleans are `0` or `1`. Values are literal text after
`=`; do not add quotes or escapes. Spaces, `#`, `=` and backslashes inside a value
are preserved. Comments occupy their own lines, starting with `#` or `;`.
Wi-Fi supports 1–32 SSID bytes and 8–63 passphrase bytes. ADB is off by default.

Unknown keys, comments and selections for absent tweaks survive menu saves,
so future settings can use the same file. A save writes and syncs a temporary
file before replacing the original. Invalid syntax, duplicate keys, unsupported
formats and oversized files are rejected without overwriting the file. An unreadable
configuration keeps the boot menu open with ADB and autolaunch disabled.

On first use, Dashborne imports `Dashborne/settings.conf` and
`Dashborne/wifi.conf` if the root file is absent. The installer and runtime retire
those old files only after saving the unified configuration. An existing root file
always takes precedence. The older ADB Enabler Wi-Fi file can supply credentials
when no Dashborne Wi-Fi file exists; it never enables developer access by itself.
The installer's `--wifi-config` JSON option updates `wifi.ssid` and `wifi.password`
without replacing other preferences.

`EasyUI.cfg` remains the stock runtime's startup selector. One-use launch/recovery
markers are temporary control state, not a second preferences file.

## Load an app from microSD

Open **Loader**, enter the app's folder, tap its `.so` file, then tap **Load app**.
Directories appear first. Selecting a file does not execute it. Unsupported files
remain visible, but cannot be launched. The browser stays within the microSD root
and shows up to 256 entries per folder; larger folders display a limit message.

This launches native DASH/FlyThings apps: ARMv7 Linux hard-float shared libraries
exporting `onEasyUIInit`, `onStartupApp`, and `onEasyUIDeinit`. A Dashborne extension
is a different interface and belongs in **Patches**. Standalone Linux executables,
shell scripts, and Android APKs are not supported by this launcher.

To start an app automatically, select it and enable **Autolaunch** in Loader.
The chosen path and switch are saved in `dashborne.conf`. Disable the switch to
resume the official dashboard at boot. To choose a different automatic app,
disable Autolaunch, select the new app, and enable it again.

The three-second splash remains available: tap to enter configuration before
an automatic launch. **Continue booting** follows the saved autolaunch choice;
**Load app** starts the current selection once without changing that choice.
Missing/unsupported apps and failed handoffs return to configuration. If an app
crashes and the UI service restarts, the return marker prevents an immediate
retry loop. Autolaunch is tried again on the next power cycle.

Keep the app's resources and dependencies on the card in the paths expected by
that app. The runtime resource root remains `/res/ui/`; apps can reference their
own SD layouts with `../../../mnt/extsd/...`.
Loading an app does not apply the selected dashboard patches.

The loader validates the selected ELF, then writes a one-use request in `/tmp`
and restarts only the UI service. The new process consumes the request **before**
loading the library, delegates initialization/startup/shutdown to that app, and
keeps its module resident. A failed load returns to Loader. If the UI service
restarts after the app exits or crashes, the consumed request brings it back to
Dashborne configuration instead of selecting the app again. A hung app still
requires a restart or power cycle.

`EasyUI.cfg` and the installed OS stay unchanged. Power cycling clears a one-use
selection; the saved autolaunch preference still applies to the next boot.

## ADB connection handling

Enable ADB manages a developer connection only inside configuration. The controller
confirms the previous TCP daemon has stopped, validates Wi-Fi settings in `dashborne.conf`, requests station
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
sh dashborne/build.sh
sh dashborne/test.sh
python3 dashborne/install.py --device DEVICE_IP:5555
```

For a fresh or existing mounted card (the Wi-Fi JSON is optional):

```sh
python3 dashborne/install.py --card /Volumes/CARD --wifi-config /path/to/wifi.json
```

The installer sets `startupLibPath` to
`/mnt/extsd/Dashborne/lib/libdashborne.so`, preserves existing configuration
fields (or creates the tested DASH defaults for a fresh card), and saves the previous selection in `Dashborne/previous.cfg`. Reinstalling
preserves saved patch choices and Wi-Fi settings. Uploads are read back and
compared before activation.

Use `--configure` to open configuration once, bypassing the splash. A manually
created `Dashborne/config.once` file has the same effect and is consumed on start.
Development previews use `--preview` with an automatic four-minute reboot to the
previous startup selection; `--keep` retains the preview as the normal startup.

## Patch files

Patch definitions live in [`patches/definitions`](patches/definitions). The build
compiles each definition to a small `.dbp` and generates `patches/catalog.tsv`.
Copy the compiled files and catalog together to `Dashborne/patches` on the card.
The loader discovers entries from that catalog on startup.

Entries can be versioned layout patches or [native extensions](extensions/README.md).
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
python3 dashborne/install.py --device DEVICE_IP:5555 --restore
```

The same `--restore` option works on a mounted card while the DASH is off. Official
app preferences under `/data` are managed by the vendor app; restoring the loader
selection does not reset those preferences.

## Development checks

From the repository root, run `sh test.sh` after building. It exercises startup,
touch controls, Loader handoffs and autolaunch, Device inventory, configuration
migration/preservation, patch composition, extension lifecycle and the bundled
extensions. Native checks use AddressSanitizer and UndefinedBehaviorSanitizer.
ADB checks include delayed/failed service operations and 10,000 deterministic
fault/toggle events. Hardware observations are recorded in [research](../docs/research.md).

## Device information

The Device page reads OS properties, `/proc`, `/sys`, and selected app settings.
The official dashboard version is identified by the complete installed library's
size and CRC32 (currently 6.11.2 and 6.11.4); it is never inferred from a downloaded
update or a selected patch. The hardware ID comes from the installed runtime's
read-only device-ID API. The DASH serial uses the same prefix, UUID word sum and
production date formatting as stock Settings. Component firmware fields use bounded snapshots of the
recognized app's reported values, and remain unavailable before it has received
them. Production stamp is the app's stored `proddate` value. Board vendor is the
OS image's reported board vendor, which is distinct from the product brand.

Memory values reflect Linux-usable RAM. Storage capacity is the whole medium,
not the mounted partition's free space. Panel mode is the kernel's physical mode
before the UI's rotation. The inventory does not change device configuration.
