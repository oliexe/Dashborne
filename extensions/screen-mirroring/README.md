# Enable Screen Mirroring

Screen Mirroring adds a receiver selector to the **Extra** menu. AirPlay is the
default; Miracast is experimental. Both use the same full-height viewer and
aspect controls. Back returns to Extra and requests CarPlay/Android Auto again.

## Use

1. Enable this extension under **Extensions** in Dashborne, then launch the dashboard.
2. Tap the star at the bottom right, select **Screen Mirroring**, and tap **Open**.
3. Choose **AirPlay** or **Miracast (experimental)**, then **Start mirroring**.
4. For AirPlay, join the DASH hotspot on your iPhone and select the DASH in
   Control Center's Screen Mirroring list. For Miracast, use your sender's
   Wireless Display or Miracast control. The DASH switches to Wi-Fi Direct;
   joining its usual hotspot is not the connection step for this mode.
5. Use the upper-left aspect icon to cycle through Full width, 16:9, 4:3,
   9:16, and 9:19.5. Each preset uses the full 480-pixel height. A brief hint
   identifies the selected ratio.
6. Tap the lower-right back arrow to restore stock projection and return to Extra.
   To change protocols, reopen Screen Mirroring.

Only one receiver runs at a time. Opening the selector does not change projection.
AirPlay is selected each time the selector opens. The aspect preset lasts until
the plugin reloads; it can change during playback without reconnecting. Source
aspect detection is not implemented.

The upper-right retry arrow appears while disconnected. Network transitions have
a 30-second timeout. Back waits for the hotspot to return after Miracast before
requesting stock projection. If restoration fails, the viewer shows an error
and Back can retry. Miracast stays unavailable when its compatibility checks fail;
AirPlay can still be used.

The screen stays awake while mirroring is active; manual screen-off takes
priority. The plugin uses the official network manager and does not enable ADB
or change saved Wi-Fi settings. Sender touch control and protected-video playback
are not provided.

## Implementation

The supported installed apps are **6.11.2 and 6.11.4**. Full-file fingerprints, exported
function addresses, and original instruction checks guard the runtime changes.
Miracast adds checks for the Wi-Fi Display receiver and P2P service.

The plugin registers its Extra entry and native Activity through the host API.
Its microSD layout contains a receiver selector, a `ZKVideoView`, and compact
Back/Retry/Aspect icons with 80×80 touch targets. During playback, small overlay
windows keep the icons above the video without a permanent header.

AirPlay selects startup mask `0x400`; Miracast selects `0x800`. Stock uses `0x12`.
Miracast also temporarily permits Wi-Fi Direct in the official projection restart
queue, while retaining the block for station mode. Both changes are guarded,
aligned instruction writes with cache synchronization and retryable restoration.

Miracast requests P2P mode through the official network manager, waits for it to
be active, and then requests the receiver restart. The session accepts only its
selected protocol and a new, ready decoder generation. AirPlay retains its
1888×384 geometry check; Miracast accepts negotiated video dimensions. Aspect
presets resize the native opening and the rotated hardware video plane together.

Back, pause, and cleanup hide the video, release the owned display inhibitor,
and restore the instruction changes. Miracast queues hotspot restoration before
stock projection restarts. Pending restoration or live Activity callbacks prevent
plugin unloading. Vehicle-occupancy flags and saved official settings are unchanged.

## Build and validation

```sh
python3 extensions/screen-mirroring/generate.py
sh extensions/screen-mirroring/test.sh
DASHBORNE_CC=/opt/homebrew/opt/llvm/bin/clang \
DASHBORNE_LD=/opt/homebrew/opt/lld/bin/ld.lld sh dashborne/build.sh
```

Host tests cover exclusive protocol selection, stale frames, negotiated geometry,
network startup and restoration timeouts, early Back, partial patch failures,
retry, manual screen-off, and Activity cleanup. AirPlay tests cover live aspect
changes, resize failures, reconnects, and rotated display coordinates.

AirPlay playback and Back restoring CarPlay/Android Auto were confirmed on the
DASH before adding the selector. The combined build still needs hardware testing:
Miracast discovery, pairing, video, audio, and repeated switching are unverified.

On 6.11.4, activation, the receiver selector and return to Extra were checked
on the device. Phone-to-receiver playback still needs a check on this version.
