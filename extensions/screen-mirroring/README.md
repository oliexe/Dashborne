# Enable Screen Mirroring

Adds **Screen Mirroring** to the official Settings categories. Selecting it
opens a separate viewing window and switches the bundled projection service
to AirPlay. **Back** closes that window, returns to the previous Settings page,
and requests the original CarPlay/Android Auto mode again.

## Use

1. Enable this plugin in Dashborne → Patches, then launch the dashboard.
2. Open the official Settings and select **Screen Mirroring** in the left list.
3. Join the DASH hotspot on the iPhone. In Control Center → Screen Mirroring,
   select the DASH.
4. Tap **Aspect** to cycle through **Full width → 16:9 → 4:3 → 9:16 → 9:19.5**.
   The last two presets suit portrait screens. Narrower presets center the picture
   with black space on either side. Full width keeps the original wide presentation.
5. Tap **Back** to return to Settings and restore CarPlay/Android Auto.

Aspect changes apply live without reconnecting the iPhone. These are manual
display proportions, not automatic source-aspect detection; choose the preset
that suits the phone's orientation. The selection survives Retry and reopening
the window within the same dashboard session, and resets to Full width after
the plugin reloads.

The receiver takes a few seconds to switch modes. **Retry** restarts a failed
or disconnected mirroring attempt. The screen remains awake while this window
is open; manual screen-off takes priority. The plugin does not configure Wi-Fi
or enable ADB. The official projection service manages the hotspot and pairing.
Touch control of the iPhone and protected-video playback are not provided.

Selecting the plugin only adds the Settings entry. The dashboard's existing
CarPlay/Android Auto widget and projection mode stay unchanged until the
mirroring window opens. The previous widget cover patch and two-minute
discovery window have been removed.

## Implementation

The supported installed app is 6.11.2. Full-file size/CRC guards cover the app,
UI runtime, projection library, receiver daemon and AirPlay library. Exported
function addresses and the original startup-mask instruction are also checked.

The Settings list adapters append an action row and delegate all existing rows.
They can chain with Contrast in either load order. Cleanup restores the saved
callbacks in reverse extension order and refuses to overwrite a later hook.
No Settings layout overlay is needed.

The separate native Activity uses an original microSD layout with a header,
Back/Retry/Aspect controls and a `ZKVideoView` that exposes the hardware video plane.
The host registers it through a resident factory trampoline, so an unloaded
plugin never leaves a dangling constructor. Live activities and queued opens
must finish before the plugin can be unloaded.

On entry, an aligned instruction changes the next receiver startup mask from
`0x12` to `0x400`. The plugin requests startup and a forced restart through the
official projection queue, with a 1888×384 viewport. It observes the official
video status, requests playback after AirPlay connects, and presents only a
new AirPlay generation with decoded frames and matching dimensions. Old
CarPlay frames cannot be shown while a restart is pending. The video plane is
placed at physical `(16, 16, 384, 1888)` for the rotated panel in Full width mode.
Aspect presets resize both the transparent native view and the hardware plane,
leaving the negotiated receiver viewport unchanged. A failed plane resize is
retried on the next tick; the view keeps its previous geometry until success.

Back, Activity pause and cleanup hide the video, release the owned display
inhibitor (`0x80`), restore the original instruction, and request a stock restart
using the dashboard widget's original size. A rapid Back supersedes the queued
AirPlay restart. Restore failures remain retryable and block unsafe unloading.
No vehicle-occupancy flags or saved official settings are changed.

## Build and validation

```sh
python3 extensions/screen-mirroring/generate.py
sh extensions/screen-mirroring/test.sh
sh extensions/contrast/test.sh
sh app/extensions/test.sh
sh app/dashborne/test.sh
DASHBORNE_CC=/opt/homebrew/opt/llvm/bin/clang \
DASHBORNE_LD=/opt/homebrew/opt/lld/bin/ld.lld sh app/dashborne/build.sh
```

Host tests cover guarded instruction changes and rollback, stale video,
protocol/geometry checks, decoder readiness, disconnect/reconnect, rapid Back,
manual screen-off, failed startup and failed restoration. Aspect tests cover
live cycling without receiver restarts, resize failures, reconnects, preset
retention, rotated coordinates and header separation. Settings and window
callback tests cover row delegation, secondary-base adjustments, duplicate taps,
closing timers and live-callback cleanup. Engine tests exercise factory lifetime
across unloading/reactivation and partial activation failures. The native
loader and Contrast regression tests pass, as does ARM cross-compilation.

The updated loader, Contrast adapter, mirroring plugin, manifest, layout and
catalog are installed on microSD and verified by read-back. The loader restarted
into configuration and ADB reconnected.
The subsequent aspect update replaces only the mirroring plugin and its layout;
both files were verified by read-back while configuration was open.

The dedicated window's video display and Back returning to Settings and
CarPlay/Android Auto were confirmed on hardware. The new aspect presets still
need a hardware check. Audio and repeated switching have not been separately
verified.
