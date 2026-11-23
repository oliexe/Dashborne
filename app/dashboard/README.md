# First Dashborne screen

A 1920 × 480 screen with a touch counter and an **Open original dashboard**
button, plus USB and Wi-Fi ADB debug switches. It runs inside the installed FlyThings process
and forwards the original initialization and shutdown callbacks. Display and touch
have been verified on the DASH. Vehicle data is not connected yet.

## Build and check

Requires Python 3, LLVM Clang and LLD with ARM support. On macOS with Homebrew:

```sh
DASHBORNE_CC=/opt/homebrew/opt/llvm/bin/clang \
DASHBORNE_LD=/opt/homebrew/opt/lld/bin/ld.lld \
sh app/dashboard/build.sh
sh app/dashboard/test.sh
```

The output is `card/lib/libdashborne.so` and `card/ui/dashborne.ftu` under this
directory. `screen.json` is the editable layout source. The build creates its
link dependency stub locally; the running app loads `/res/lib/libzkgui.so`.

The local tests use a simulated framework with address and undefined-behavior
sanitizers. They cover lifecycle forwarding, touch counting, original-screen
navigation, allocation/layout failures and compatibility fallbacks. They also
check the compressed layout and touch-target bounds. They do not verify the
display driver, physical touch input or interactions with the live services.

## Runtime integration

The adapter registers `dashborneActivity` with the installed activity factory.
It retains the framework's activity implementation and overrides layout selection,
creation and click handling. It uses a small ABI adapter instead of a separate
graphics stack.

This first implementation targets the exact runtime captured from the test DASH:
826,052 bytes, FNV-1a-32 `3ca7071f`. That fingerprint is a compatibility check.
Missing bindings, a different runtime, or an unreadable layout select the original
startup screen. Layout creation is attempted before selecting Dashborne, allowing
a reported creation failure to fall back too. A framework crash cannot be caught
by this fallback.

Resource names are relative to the framework's resource root. The layout name
resolves from either `/res/ui/` or `/mnt/extsd/ui/` to our file on the card.
Original resource paths stay available. The screen's prepared instance is handed
to the factory; its second `BaseApp::create` call returns the already-created
window. The inspected runtime supports this path, but the complete lifecycle
still needs verification on the DASH.

Events append to `/mnt/extsd/Dashborne/events.log`. After testing, check for
`screen-created`, `dashborne-selected`, `touch`, and `open-original`.

## Physical test

To stage onto the existing test card, preserving its other configuration values:

```sh
python3 app/dashboard/stage.py /Volumes/DASH_TEST
```

The command saves the previous launcher configuration on the card. To restore it:

```sh
python3 app/dashboard/stage.py /Volumes/DASH_TEST --restore
```

1. Prepare the existing test card using the staging command above, then eject it.
2. With the DASH powered off, insert the card and power it on.
3. Check the screen orientation, tap the test button several times, and verify
   that the counter follows each tap.
4. Open the original dashboard and check its normal behavior.
5. Power off before removing the card. Starting without it selects the installed
   dashboard. Reconnect the card to the laptop to inspect the event log.

Rendering and physical touch are confirmed on hardware. Full original-dashboard
navigation and shutdown checks remain.

## USB ADB

1. Boot this build from the card and connect the DASH to the laptop with a USB
   data cable.
2. Tap **Enable USB ADB**. The status line refreshes once per second and reports
   the local daemon and USB gadget state.
3. On the laptop, run `adb devices -l`, then `adb shell` once the device appears
   with state `device`. Connection and shell access are still pending testing.

The switch requests the existing init service and sets the temporary ADB TCP port
to zero to select USB. It uses the legacy gadget trigger where available, or
ConfigFS for the `4100000.udc-controller` observed on the test DASH. It does not
change persistent debug settings. **Disable USB ADB** requests daemon shutdown
and disables or unbinds the gadget; status polling completes cleanup after init
reports the daemon stopped. The previous temporary TCP setting is then restored.
Application shutdown also requests cleanup; reboot clears the temporary settings.

The ConfigFS backend creates a `dashborne` gadget, mounts FunctionFS if needed,
and starts the installed daemon. It waits for ADB endpoints before binding the
controller. **USB configured** means the host configured the USB device;
**waiting for USB host** means the gadget is bound but no attachment is reported.
Neither message confirms an authorized ADB shell. Binding and endpoint waits are
limited to 15 status polls. Reboot if a previous app process left the debug gadget
directory behind. The test DASH binds the gadget and creates endpoints, but the Mac has not
enumerated it, including with a direct data cable.

The switch leaves an already-running daemon or an already-enabled USB gadget
alone and reports that state. Failed requests stay visible, with a retry available
for incomplete shutdown. The status line describes the device side; only the
laptop's `adb devices` result confirms a connection.

Event logs are flushed to the card after each entry. They record debug requests
and state changes alongside touch and screen lifecycle events.

## USB diagnostics

The first USB test reported that the legacy control interface could not be read.
This does not identify the connector's wiring. The downloaded application package
also contains a ConfigFS-based USB helper. The first snapshot confirmed that
helper is installed and that `/sys/class/udc/4100000.udc-controller` exists.
Several expected shell utilities were absent, so the remaining snapshot was
incomplete. Version 2 uses shell builtins and the observed `ls` utility.

Opening the Dashborne screen starts a read-only diagnostic snapshot in a child
process. Keep the screen open for about ten seconds with the USB data cable
connected to the laptop, then power off and return the card. The report is
`Dashborne/usb-diagnostics.txt`; `events.log` records when it is saved or fails.

The report covers USB device controllers, roles, ConfigFS, loaded modules,
process identity and available helpers. It neither loads drivers nor changes
USB configuration. The ADB button performs configuration separately.

## Wi-Fi ADB

Stage a local JSON configuration with `ssid` and `password` strings:

```sh
python3 app/dashboard/stage.py /Volumes/DASH_TEST --wifi-config /path/to/wifi.json
```

The staging command validates the configuration and writes `Dashborne/wifi.conf`
on the card. Booting this build requests station mode through the installed
application's networking workflow, then requests the configured connection.
The screen displays the IPv4 address reported for `wlan0` once the stock Wi-Fi
manager reports a connection. A failed connection can be retried with the Wi-Fi
button. The connection requests use the installed GNU C++11 string ABI through
a small const-reference adapter.

Tap **Enable Wi-Fi ADB**, then connect from a computer on the same network:

```sh
adb connect <displayed-ip>:5555
adb devices -l
adb -s <displayed-ip>:5555 shell
```

The button starts the existing daemon with temporary `service.adb.tcp.port=5555`.
The status reports the daemon state; successful `adb connect` and shell access
still need verification on the DASH. Disable the current ADB mode before enabling
the other one. Stopping Wi-Fi ADB restores the previous temporary port setting
once init reports the daemon stopped. Wi-Fi remains available to the original
application. No persistent debug property or authentication setting is changed.

The installed networking API is consistent with the documented
[FlyThings Wi-Fi manager](https://docs.flythings.cn/zh-hans/wifi.html).
Local tests cover config validation, partial reads, API failures, delayed mode
changes, connection timeout/retry, IPv4 discovery and ADB start/stop/port restoration.
