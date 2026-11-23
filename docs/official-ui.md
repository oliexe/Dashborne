# Official UI development copy

The official 6.11.4 update contains the display application partition: native
libraries, communication helpers, fonts, translations, layouts and artwork.
It runs with the DASH's existing Linux kernel and FlyThings runtime.

The package is retrieved through the device firmware metadata endpoint using
the cloud device ID. Its download links expire, so a fresh metadata response is
needed when an old link stops working. The inspected `update.img` is 59,769,404
bytes with SHA-256:

```text
618ca7c0db0fa99be4fbc461cfc9d86ba10496a813e713133e4c99773009bb7f
```

The ZKSWE container holds one ext-family application filesystem. Header CRC32
and payload MD5 were verified before extraction; the extracted resource tree
contains 1,528 files.

## Verification status

The downloaded package and all 1,528 files read back from the SD card match
their recorded hashes. The preview launcher builds and its callback, reconnect,
timeout and shutdown tests pass. On-device logs confirmed entry into the official
initialization and a Wi-Fi reconnect during an earlier preview attempt.

The installed official 6.11.2 application was launched through the forwarding
launcher, using the original `/res` filesystem. The device owner confirmed that
the normal official dashboard rendered. Wi-Fi ADB did not return during this
check, so remote debugging alongside the official screen remains unresolved.

Rendering from the downloaded 6.11.4 SD copy is not yet confirmed. Starting it
disrupted Wi-Fi ADB; a subsequent attempt left the screen blank or frozen. A
physical reboot recovered ADB Enabler and remote access before the installed
application was tested. The downloaded copy has not been flashed.

## SD-backed application resources

The working copy is at `/mnt/extsd/Dashborne/official-6.11.4/res`. A temporary
bind mount presents this tree at `/res`, keeping the app's absolute paths
consistent across libraries, helpers, UI assets, fonts and translations.
The preview launcher loads `/res/lib/libzkgui.so` and forwards all three app
callbacks. It returns the official startup activity, `dash1actActivity`, without
registering a replacement screen.

A preview session saves the card's existing `EasyUI.cfg`, stops `zkswe`, mounts
the SD resource tree, selects the official library and starts `zkswe`. Once the
app has loaded, it restores the saved card configuration for the next startup.
The test session used a recovery timer to return to the helper when the laptop
could not confirm the preview. It restored access in earlier attempts but did
not restore access in the final SD-resource attempt. The installed-resource
test subsequently returned to ADB Enabler when its recovery timer expired.

During official startup, the debug connection was lost; checks initially found
the device reachable with TCP port 5555 refusing connections. The small
[`official-ui` launcher](../app/official-ui/launcher.c) waits for that startup,
then requests the development Wi-Fi connection from `adb-enabler/wifi.conf` and
attempts to start TCP ADB using the helper's existing adapters. Its worker exits
once Wi-Fi and the ADB listener are ready, or after a bounded attempt. It does not replace
the official screens or continuously override later network choices.

Rebooting clears the temporary mount. The session scripts restore ADB Enabler
as the card startup selection; this recovery path worked after the SD preview.
During the preview, `/res` refers to the SD copy; file changes there affect that copy.
Application preferences and pairing state continue to use `/data`.

## Modification points

- `ui/*.ftu`: compressed JSON layouts. The existing Dashborne layout decoder can
  read the official dashboard and editor screens. Preserve control IDs and
  hierarchy when changing positions, typography or artwork references.
- `ui/dash/`, `ui/Knob/` and related directories: visual assets used by the app.
- `lib/libzkgui.so`: compiled screen and widget behavior. Layout edits retain
  that behavior; adding behavior requires a compatible code adapter or widget
  integration. The package does not contain the app's source project.
- `/data/pagelayout.json`: user page configuration, separate from static screen
  layouts. `/data/zkapp.cfg` and `/data/preferences.json` contain app settings.

The official UI is the baseline for this development path. The separate
[simulated cluster](../app/dashboard/README.md) remains available as a visual
prototype.

## Build the preview launcher

```sh
DASHBORNE_CC=/opt/homebrew/opt/llvm/bin/clang \
DASHBORNE_LD=/opt/homebrew/opt/lld/bin/ld.lld \
sh app/official-ui/build.sh
sh app/official-ui/test.sh
```

The output is `app/official-ui/build/libofficial-preview.so`. Place it at
`/mnt/extsd/lib/libofficial-preview.so` and select that path in the temporary
preview configuration. For an SD-resource preview, the resource mount must be
in place before starting the UI service. Without that mount, the launcher uses the installed
official resources, which is the configuration confirmed to render. The normal
SD startup selection remains ADB Enabler.
