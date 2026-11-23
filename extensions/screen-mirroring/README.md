# Enable Screen Mirroring

Enables the DASH's bundled AirPlay receiver through its official projection
service. This is an experimental plugin: receiver discovery and live iPhone
video/audio still need hardware confirmation.

## Use

1. Select **Enable Screen Mirroring** in Dashborne → Patches, then launch the dashboard.
2. Enable the official app's phone projection feature and show its projection widget.
3. Join the DASH's Wi-Fi hotspot from the iPhone. Open Control Center →
   **Screen Mirroring**, then look for the DASH's configured device name.
4. Stop mirroring on the iPhone before switching back to CarPlay.

The plugin does not manage Wi-Fi or enable ADB. Hotspot setup, receiver name,
video decoding and audio routing remain with the official projection service.
CarPlay and Android Auto remain enabled in its capability mask; simultaneous
sessions and switching between them are not yet validated. Touch control of the
iPhone and playback of protected video are not provided by this plugin.

To disable it, reboot into Dashborne configuration, turn the plugin off and launch
the dashboard. Booting without the card also restores the original startup mask.

## Implementation

The verified official 6.11.2 app passes a 76-byte configuration to
`lylinkapi_start`. Its function mask at offset `0x18` is `0x12`.
The bundled `lylinkapp_ab` dispatcher starts its AirPlay receiver when bit
`0x400` is present. The plugin changes that startup mask to `0x412` using one
aligned ARM instruction in the process's private executable mapping. It restores
the mapping's protection and flushes the instruction cache after each change.
There are no plugin callbacks in the projection worker.

Full-file size/CRC checks cover the installed app, projection UI library, AB
daemon and AirPlay library before any modification. Symbol addresses, mapping
permissions and the original instruction are checked too. A different build is
rejected. Activation requires projection to be stopped; normally it has not yet
started while Dashborne configuration is open.

Cleanup restores the original instruction. If projection is running, it requests
a restart through the official app's existing queue. A failed memory/cache/
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
synchronization and cleanup retries under ASan/UBSan. ARM cross-build passes.
The projection library, daemon and AirPlay library read from the test DASH match
the copies inspected in the 6.11.4 update package.
The plugin binary, manifest and catalog were deployed to microSD and read back
byte-for-byte. Activation and iPhone discovery/streaming remain unconfirmed.
