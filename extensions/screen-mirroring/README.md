# Enable Screen Mirroring

Enables the DASH's bundled AirPlay receiver through its official projection
service and displays mirroring in the existing phone projection widget.
The plugin toggle selects the projection mode for the next dashboard launch.

## Use

1. Select **Enable Screen Mirroring** in Dashborne → Patches, then launch the dashboard.
2. Enable the official app's phone projection feature and show its projection widget.
3. Join the DASH's Wi-Fi hotspot from the iPhone. Open Control Center →
   **Screen Mirroring**, then look for the DASH's configured device name.
4. To return to CarPlay/Android Auto, stop mirroring, reboot into Dashborne
   configuration, turn this plugin off and launch the dashboard.

The plugin does not manage Wi-Fi or enable ADB. Hotspot setup, receiver name,
video decoding and audio routing remain with the official projection service.
While selected, AirPlay is the only enabled projection protocol, preventing
CarPlay auto-reconnection from taking over its video. When unselected, the
original CarPlay/Android Auto startup mask is used. This switch is in Dashborne
configuration; there is no live mode switch in the official dashboard yet.
While AirPlay video is visible on the dashboard, the plugin inhibits automatic
display sleep. Leaving the dashboard, stopping mirroring or manually turning
the display off releases that inhibition.
Touch control of the iPhone and playback of protected video are not provided.

To disable it, reboot into Dashborne configuration, turn the plugin off and launch
the dashboard. Booting without the card also restores the original startup mask.

## Implementation

The verified official 6.11.2 app passes a 76-byte configuration to
`lylinkapi_start`. Its function mask at offset `0x18` is `0x12`.
The bundled `lylinkapp_ab` dispatcher starts its AirPlay receiver when bit
`0x400` is present. The plugin changes that startup mask to `0x400` using one
aligned ARM instruction in the process's private executable mapping. It restores
the mapping's protection and flushes the instruction cache after each change.
There are no plugin callbacks in the projection worker.

The dashboard's existing timer, resume and pause callbacks also maintain a
dedicated `DisplayController::inhibitDisplayOff` bit (`0x80`) while visible
AirPlay video is active. Stock inhibitor bits are preserved. Both timer vtable
entries, including the secondary base thunk, are guarded against their verified
symbol addresses. Original callback results and the timer's `this` adjustment
are preserved. Manual display-off retains priority; no Wi-Fi watchdog or new
background worker is introduced.

The same UI callbacks bridge AirPlay into the official `ProjectionManager`
resource-grant state. The stock CP/AA session notifications normally supply this
state; AirPlay can deliver decoded frames without it, leaving the opaque standby
cover over the video layer. While decoded AirPlay is visible and a projection
widget exists, the plugin grants presentation and calls the original cover
refresh method. It preserves the other UI gates. Disconnect, pause, explicit
screen-off and cleanup release the temporary grant. The manager mapping,
exported method anchor and private refresh entry are checked before use.

Full-file size/CRC checks cover the installed app, projection UI library, AB
daemon and AirPlay library before any modification. Symbol addresses, mapping
permissions and the original instruction are checked too. A different build is
rejected. Activation requires projection to be stopped; normally it has not yet
started while Dashborne configuration is open.

Cleanup releases the temporary presentation grant and owned sleep inhibitor,
then restores the dashboard callbacks
and their mapping permissions before restoring the original instruction.
If projection is running, it requests a restart through the official app's
existing queue. A failed memory/cache/
protection restore remains a cleanup failure so the engine will not silently
unload it. A process restart discards the private mapping regardless.

## Validation

```sh
sh extensions/screen-mirroring/test.sh
DASHBORNE_CC=/opt/homebrew/opt/llvm/bin/clang \
DASHBORNE_LD=/opt/homebrew/opt/lld/bin/ld.lld sh app/dashborne/build.sh
```

Host tests exercise incompatible instructions, duplicate activation, normal
restoration, conflicting changes, failed permission changes, failed cache
synchronization and cleanup retries under ASan/UBSan. Callback tests cover
AirPlay visibility, other protocols, disconnect, explicit display-off,
pause/resume, secondary-base dispatch, callback return values, preserving
other inhibitors, conflicting hooks and failed permission restoration/retries.
Presentation tests cover missing/removed widgets, stock UI gates, avoiding
repeated refreshes, disconnect, preserving existing grants and later revocation,
and repeated cleanup. ARM cross-build passes.
The projection library, daemon and AirPlay library read from the test DASH match
the copies inspected in the 6.11.4 update package.
The plugin binary, manifest and catalog were deployed to microSD and read back
byte-for-byte. The initial combined-protocol build (`0x412`) was confirmed to
appear in iPhone Screen Mirroring, accept a connection and briefly show mirrored
video in the projection widget, before returning to CarPlay. The AirPlay-only
build was then installed and verified by read-back.

A subsequent capture showed AirPlay reaching video decoder startup, followed
about 0.8 seconds later by the official display controller requesting projection
disable. Bluetooth and the hotspot shut down next, interrupting the stream.
The user confirmed no projection or network settings were changed. Static
inspection traced this request to the display-off branch of
`DisplayController::updateDisplayState`; this motivated the visible-video
sleep inhibitor. The iPhone's “no internet” message alone does not identify the
cause of a disconnect.

With sleep inhibition installed, the user reported a sustained connection but
no visible picture. Captures confirmed decoded 496×720 frames and a video layer
under the framebuffer, while the UI kept its opaque standby cover. This led to
the presentation-grant bridge. Another capture also showed an official
projection restart during a session; that reconnect behavior remains under
investigation.

The presentation build passes host checks and ARM cross-compilation. Its binary,
manifest and catalog are installed on microSD and verified by read-back. Visible
mirroring, audio, manual screen-off and switching back to stock mode still need
confirmation on hardware.
