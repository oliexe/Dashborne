# ADB Enabler

A minimal microSD startup app for remote DASH development. It connects to the
configured Wi-Fi network, starts the installed ADB daemon on TCP port 5555, and
shows its address and status in the center of the screen. No button press is
needed. The screen has no USB controls or dashboard features.

The helper uses the existing Linux OS, FlyThings runtime and networking service.
It builds as `libadb-enabler.so`, separately from Dashborne. It checks the same
runtime fingerprint as the tested dashboard adapter and forwards the original
application's initialization callbacks.

## Build and test

```sh
DASHBORNE_CC=/opt/homebrew/opt/llvm/bin/clang \
DASHBORNE_LD=/opt/homebrew/opt/lld/bin/ld.lld \
sh app/adb-enabler/build.sh
sh app/adb-enabler/test.sh
```

Outputs are `card/lib/libadb-enabler.so` and `card/ui/adb-enabler.ftu` under this
directory. The helper reuses the Wi-Fi adapter from `app/dashboard`, with its own
configuration path. It does not link the USB debug or ConfigFS implementations.

## Wi-Fi configuration

On the SD card, `adb-enabler/wifi.conf` contains two lines: SSID, then WPA
passphrase. Both lines end with a newline. See [wifi.conf.example](wifi.conf.example).
The password is never displayed. An unavailable network is retried automatically;
configuration changes are reread on retry or after restarting the helper.

The installer also accepts a local JSON file with `ssid` and `password` fields
and converts it to the card format. Install over an existing ADB connection:

```sh
python3 app/adb-enabler/install.py --device <ip>:5555 --wifi-config /path/to/wifi.json
```

Or install on the DASH card containing `EasyUI.cfg` mounted on the laptop:

```sh
python3 app/adb-enabler/install.py --card /Volumes/DASH_TEST --wifi-config /path/to/wifi.json
```

For later installs, omit `--wifi-config` to retain the helper's current settings.
The installer preserves the previous launcher configuration in
`adb-enabler/EasyUI.previous.cfg`. Over ADB, it verifies each uploaded file by
reading it back, selects the helper only after its files are present, and restarts
the UI. For a mounted card, eject it before inserting it into the powered-off DASH.

## Development workflow

Boot with the card inserted, wait for **Wi-Fi ADB ready**, then connect to the
address shown. That status requires both a running ADB service and a listening
TCP socket on port 5555. The helper starts a stopped daemon and recognizes an
existing TCP session. It leaves a daemon using another transport alone and shows
that condition on screen.

ADB remains running when the helper exits so development UI replacements can
keep their remote connection. The helper only sets the temporary TCP port and
starts the existing init service; it does not set persistent debug properties.

To return to the launcher selected before installing the helper:

```sh
python3 app/adb-enabler/install.py --device <ip>:5555 --restore
```

Use the install command again to select ADB Enabler. Dashborne's library, layout
and Wi-Fi configuration stay on the card throughout this workflow.

## Verification

Local tests cover runtime fallback and lifecycle handling, automatic TCP startup,
an existing daemon, other transports, start failures and retries, listener
parsing, centered screen bounds, configuration validation and launcher restoration.
On hardware, screen capture confirmed the centered UI, and stopping the daemon
confirmed automatic ADB recovery without touching the screen. A full reboot
then confirmed that the SD-selected helper reconnects Wi-Fi and starts ADB
automatically; the laptop reconnected with a new kernel boot ID.
