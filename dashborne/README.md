# Dashborne

Dashborne is the microSD startup app, configuration screen and temporary extension
loader for the official S3XY DASH UI.

## Startup

The splash shows the centered logo, animated loading dots, and
**Tap screen to enter boot option** at the bottom for three seconds. Tapping anywhere on the splash cancels it and opens
configuration; there is no further timeout while configuring. Otherwise,
Dashborne applies the selected extensions and opens the official startup activity
from the installed app in flash.

Choose a category from the left sidebar to open its page. **Extensions** opens by
default. Switching pages keeps your selections and list position. The footer
shows the version from `VERSION` and the repository URL beneath the logo.

Configuration includes:

- ADB / Enable ADB: developer opt-in, off by default; enable/disable TCP
  ADB. The page uses aligned rows for the switch, live status, IP address with
  port, and configured developer Wi-Fi name. Address and network fields clear
  when ADB is disabled.
- Extensions: swipe the native list to browse up to 64 entries. Tap a row to
  toggle its extension; each row has a title, description and switch. Incompatible
  extensions have a disabled switch. Selections save immediately, and the header
  shows how many extensions are enabled.
- Loader: browse microSD folders, select an app, and tap **Load app**.
  **Up** navigates to the parent folder and **Refresh** rescans the current folder.
- Device: a scrollable table of software versions, device identifiers, board
  details, memory, uptime, display mode and storage serials/capacities. Values are
  read when the page opens; **Refresh** updates the snapshot. Missing values show
  **Unavailable**, and unrecognized dashboard binaries are identified as such.
- Continue booting: save selections, load the extensions, open the official UI.

## Configuration file

The boot menu and installer save persistent settings in
`/mnt/extsd/dashborne.conf` at the microSD root.

```ini
format=1
adb.enabled=0
adb.force=0
adb.auto_restart=0
wifi.ssid=Your Wi-Fi name
wifi.password=Your Wi-Fi password
tweak.widget-example.enabled=0
tweak.screen-mirroring.enabled=0
loader.autolaunch=0
loader.app=
```

Keys are namespaced and booleans are `0` or `1`. Values are literal text after
`=`; do not add quotes or escapes. The parser preserves spaces, `#`, `=` and
backslashes inside values. Comments occupy their own lines, starting with `#` or
`;`. Wi-Fi supports 1–32 SSID bytes and 8–63 passphrase bytes. ADB is off by
default.

Saving keeps unknown keys, comments and selections for absent extensions. The writer
syncs a temporary file before replacing the original. It rejects invalid syntax,
duplicate keys, unsupported formats and oversized files without overwriting them.
If Dashborne cannot read the configuration, it keeps the boot menu open with ADB
and autolaunch disabled.

On first use, Dashborne imports `Dashborne/settings.conf` and
`Dashborne/wifi.conf` if the root file is absent. The installer and runtime retire
those old files only after saving the unified configuration. An existing root file
always takes precedence. The older ADB Enabler Wi-Fi file can supply credentials
when no Dashborne Wi-Fi file exists; it never enables developer access by itself.
The installer's `--wifi-config` JSON option updates `wifi.ssid` and `wifi.password`
without replacing other preferences.

`EasyUI.cfg` remains the stock runtime's startup selector. Launch/recovery
markers hold one-use control state.

## Load an app from microSD

Open **Loader**, enter the app's folder, tap its `.so` file, then tap **Load app**.
Directories appear first. Selecting a file does not execute it. Unsupported files
remain visible, but cannot be launched. The browser stays within the microSD root
and shows up to 256 entries per folder; larger folders display a limit message.

This launches native DASH/FlyThings apps: ARMv7 Linux hard-float shared libraries
exporting `onEasyUIInit`, `onStartupApp`, and `onEasyUIDeinit`. A Dashborne extension
is a different interface and belongs in **Extensions**. Standalone Linux executables,
shell scripts, and Android APKs are not supported by this launcher.

To start an app automatically, select it and enable **Autolaunch** in Loader.
Dashborne saves the chosen path and switch in `dashborne.conf`. Disable the switch to
resume the official dashboard at boot. To choose a different automatic app,
disable Autolaunch, select the new app, and enable it again.

The three-second splash remains available: tap to enter configuration before
an automatic launch. **Continue booting** follows the saved autolaunch choice;
**Load app** starts the current selection once without changing that choice.
Missing/unsupported apps and failed handoffs return to configuration. If an app
crashes and the UI service restarts, the return marker prevents an immediate
retry loop. Dashborne retries autolaunch on the next power cycle.

Keep the app's resources and dependencies on the card in the paths expected by
that app. The runtime resource root remains `/res/ui/`; apps can reference their
own SD layouts with `../../../mnt/extsd/...`.
Loading an app does not apply the selected dashboard extensions.

The loader validates the selected ELF, then writes a one-use request in `/tmp`
and restarts only the UI service. The new process consumes the request before
loading the library, delegates initialization/startup/shutdown to that app, and
keeps its module resident. A failed load returns to Loader. If the UI service
restarts after the app exits or crashes, the consumed request brings it back to
Dashborne configuration instead of selecting the app again. A hung app still
requires a restart or power cycle.

`EasyUI.cfg` and the installed OS stay unchanged. Power cycling clears a one-use
selection; the saved autolaunch preference still applies to the next boot.

## ADB connection handling

Enable ADB normally manages a developer connection inside configuration. The
controller confirms the previous TCP daemon has stopped, validates Wi-Fi
settings in `dashborne.conf`, turns Wi-Fi off, waits for teardown, then connects
in station mode. Each enable starts a fresh connection, including when the
previous driver state still reports connected. The toggle shows your saved
choice; the status reports connection progress or failure.

Before showing an address, Dashborne checks the interface, association and SSID
where the driver supports them, a fresh ARP reply from the local router, and a
TCP 5555 listener. The router check uses nonblocking packets and refreshes every
eight seconds. A failed check clears the address and triggers a bounded Wi-Fi
reset. It does not require internet access.

**ADB listening; LAN verified** means the local listener and router path passed.
**ADB client connected** means an incoming TCP connection is established. Router
client isolation can still block a computer even when the router itself replies.

The controller clears stale displayed addresses, retries failed Wi-Fi
connections and missing ADB listeners up to three times, then reports a failure.
Toggle off/on to retry after correcting the cause. Force ADB retries after a
60-second pause so a temporary network outage does not require a manual toggle.
Disabling cancels connection
retries, stops TCP ADB, and reports **Disabled** only after the service confirms
it stopped. Stop failures remain visible. For an ADB daemon using another
transport, the controller reports that transport and leaves the daemon running.

If saving an enabled selection fails, ADB stays off. A disable request takes
effect even if saving fails, with the card-write error shown. Dashborne cancels
pending work at dashboard handoff unless Force ADB is selected, then leaves
networking to the official app.

**Force ADB** (`adb.force=1`) keeps the connection controller running after the
official dashboard opens. A reversible, in-memory hook blocks the stock radio
startup resets and mode changes that would replace developer Wi-Fi with the projection hotspot.
It also suspends the stock routine that periodically stops the ADB daemon.
The developer controller can still reset and reconnect the radio. This option
requires Enable ADB; disabling either restores the stock radio and ADB maintenance code.
On unsupported software, dashboard handoff disables Force ADB and Auto recovery
and continues with stock networking.

**Auto recovery** (`adb.auto_restart=1`) requires Force ADB. A separate supervisor
watches a heartbeat from the UI event loop. If the process exits or the heartbeat
stops for 20 seconds, it saves logs under `Dashborne/diagnostics/watchdog-N/`,
flushes the card and reboots Linux into configuration. A full reboot releases
display and driver resources that can remain allocated after killing the UI.
The next boot stays in configuration until you choose to continue.

Captures include recent app logs, the kernel log, memory status and available
process details, with file size limits. If Linux refuses the reboot, the
supervisor falls back to restarting the UI, limited to three attempts per boot.
It cannot recover a halted kernel.

Both options default to off and are saved in the card-root `dashborne.conf`.
The watchdog heartbeat and restart count live in RAM. Normal shutdown or
disabling developer access cancels monitoring.

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
fields (or creates the tested DASH defaults for a fresh card), and saves the
previous selection in `Dashborne/previous.cfg`. Reinstalling keeps extension choices
and Wi-Fi settings. For ADB uploads, the installer reads files back and compares
them before activation.

Use `--configure` to open configuration once, bypassing the splash. You can also
create `Dashborne/config.once`; Dashborne consumes it on startup. Development
previews use `--preview` with an automatic four-minute reboot to the previous
startup selection; `--keep` retains the preview as the normal startup.

## Extension files

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

The loader disables incompatible or missing extensions in the list. At launch, it
combines selected edits, checks for conflicts and applies them to the installed
layout. It writes `Dashborne/generated.ftu` on the card and bind-mounts it over
the original resource. With no layout patches selected, it skips this override.

Next, the engine activates enabled native extensions as a group. A patch failure
keeps configuration open with an error; an extension failure also rolls back
activation. The engine records resource mounts for cleanup and recovery after
UI restarts.

## Reversal

Power off, remove the microSD, then boot to return to the installed official
app. A reboot clears temporary mounts; Dashborne reapplies selected extensions if
its card is still inserted. The loader also clears its previous layout override
when reloaded so configuration starts from the installed base resource.

To restore the SD startup selection that preceded Dashborne and reboot:

```sh
python3 dashborne/install.py --device DEVICE_IP:5555 --restore
```

The same `--restore` option works on a mounted card while the DASH is off. The
vendor app manages its preferences under `/data`; restoring the loader selection
does not reset those preferences.

## Development checks

From the repository root, run `sh test.sh` after building. It exercises startup,
touch controls, Loader handoffs and autolaunch, Device inventory, configuration
migration/preservation, patch composition, extension lifecycle and the bundled
extensions. Native checks use AddressSanitizer and UndefinedBehaviorSanitizer.
ADB checks include delayed/failed service operations and 10,000 deterministic
fault/toggle events. See [research](../docs/research.md) for hardware observations.

## Device information

The Device page reads OS properties, `/proc`, `/sys`, and selected app settings.
It identifies the official dashboard version by the installed library's size and
CRC32, with fingerprints for 6.11.2 and 6.11.4. The hardware ID comes from the
installed runtime's read-only device-ID API. The DASH serial uses the same
prefix, UUID word sum and production date formatting as stock Settings.
Component firmware fields use bounded snapshots of values received by the
recognized app. They remain unavailable until those values arrive. Production
stamp is the app's stored `proddate` value. Board vendor is the OS image's
reported board vendor, which is distinct from the product brand.

Memory values reflect Linux-usable RAM. Storage capacity reports the size of the
whole medium. Panel mode is the kernel's physical mode before the UI's rotation.
The inventory does not change device configuration.
