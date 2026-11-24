# Enable Screen Mirroring

The plugin adds **Screen Mirroring** to Dashborne's **Extra** menu. Opening it switches
the bundled projection service to AirPlay. **Back** closes the viewer, returns
to Extra, and requests the original CarPlay/Android Auto mode again.

## Use

1. Enable this plugin under **Patches** in Dashborne, then launch the dashboard.
2. Tap the **star icon** at the bottom right of the dashboard, select **Screen Mirroring**,
   then tap **Open**.
3. Join the DASH hotspot on the iPhone. Open **Screen Mirroring** in Control Center and
   select the DASH.
4. Tap the **aspect icon at the upper left** to cycle through Full width, 16:9, 4:3, 9:16 and 9:19.5.
   The last two presets suit portrait screens. Narrower presets center the picture
   with black space on either side. Each preset uses the full 480-pixel height;
   Full width fills the 1920×480 display. A two-second hint identifies the preset.
5. Tap the **back arrow at the lower right** to return to Extra and restore CarPlay/Android Auto.

Aspect changes apply live without reconnecting the iPhone. Choose the display
proportions to suit the phone's orientation; the plugin does not detect the
source aspect ratio. The selection survives Retry and reopening the window
within the same dashboard session, and resets to Full width after the plugin
reloads.

The receiver takes a few seconds to switch modes. The **retry arrow at the upper
right** appears while disconnected and restarts a failed
or disconnected mirroring attempt. The screen remains awake while this window
is open; manual screen-off takes priority. The plugin does not configure Wi-Fi
or enable ADB. The official projection service manages the hotspot and pairing.
Touch control of the iPhone and protected-video playback are not provided.

Selecting the plugin only adds the Extra entry. The dashboard's existing
CarPlay/Android Auto widget and projection mode stay unchanged until the
mirroring window opens.

## Implementation

The supported installed app is 6.11.2. The plugin checks full-file size/CRC
fingerprints for the app, UI runtime, projection library, receiver daemon and
AirPlay library. It also checks exported function addresses and the original
startup-mask instruction.

The plugin registers a title, description and window-open callback through the
host's Extra registry. It does not hook the stock Settings list. The host removes
the entry before unloading the plugin or rolling back a failed activation.

The separate native Activity uses an original microSD layout with compact
Back/Retry/Aspect icon controls and a `ZKVideoView` that exposes the hardware
video plane. Each icon has an 80×80 touch target. The Activity creates small
overlay windows after the video view to keep controls above the transparent
video opening. The back and aspect controls stay visible; the retry control is
hidden during playback. The ratio hint hides after two seconds, leaving no
permanent header. The host registers it through a resident factory trampoline,
which disables calls into the plugin after unloading. The plugin must finish
live activities and queued opens before the engine can unload it.

On entry, an aligned instruction changes the next receiver startup mask from
`0x12` to `0x400`. The plugin requests startup and a forced restart through the
official projection queue, with a 1888×384 viewport. It observes the official
video status, requests playback after AirPlay connects, and presents only a new
AirPlay generation with decoded frames and matching dimensions. Old CarPlay
frames cannot be shown while a restart is pending. The video plane is placed at
physical `(0, 0, 480, 1920)` for the rotated panel in Full width mode. Aspect
presets resize both the transparent native view and the hardware plane, leaving
the negotiated receiver viewport unchanged. The plugin retries a failed plane
resize on the next tick; the view keeps its previous geometry until success.

Back, Activity pause and cleanup hide the video, release the owned display
inhibitor (`0x80`), restore the original instruction, and request a stock restart
using the dashboard widget's original size. A rapid Back supersedes the queued
AirPlay restart. Restore failures remain retryable and block unsafe unloading.
The plugin leaves vehicle-occupancy flags and saved official settings unchanged.

## Build and validation

```sh
# Only needed when regenerating the original icon artwork:
uv run --with resvg-py==0.5.0 python extensions/screen-mirroring/icons.py
python3 extensions/screen-mirroring/generate.py
sh extensions/screen-mirroring/test.sh
sh extensions/contrast/test.sh
sh dashborne/extensions/test.sh
sh dashborne/test.sh
DASHBORNE_CC=/opt/homebrew/opt/llvm/bin/clang \
DASHBORNE_LD=/opt/homebrew/opt/lld/bin/ld.lld sh dashborne/build.sh
```

Host tests cover guarded instruction changes and rollback, stale video,
protocol/geometry checks, decoder readiness, disconnect/reconnect, rapid Back,
manual screen-off, failed startup and failed restoration. Aspect tests cover
live cycling without receiver restarts, resize failures, reconnects, preset
retention, rotated coordinates, full-height geometry and transient ratio hints.
Window callback tests cover secondary-base adjustments, duplicate taps, closing
timers and live-callback cleanup. Engine tests exercise factory lifetime across
unloading/reactivation and partial activation failures. The tested build passed
native loader and Contrast regression tests and ARM cross-compilation.

Deployment checks compared the installed plugin, layout and icon files with
read-back copies. The loader returned to configuration and ADB reconnected.
On the DASH, the user confirmed video in the dedicated window, Back restoring
CarPlay/Android Auto, and navigation through Extra. The full-height icon layout,
audio and repeated switching still need hardware checks.
