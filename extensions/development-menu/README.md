# Development Menu

Enable **Development Menu** in Dashborne, then continue booting. Open the stock
Settings and scroll its left sidebar to **Development**; no password is needed.

The page uses the stock controls for contrast, hue, saturation, luminance,
log export, display calibration and production mode.

The extension exposes the existing sixth Settings category by changing its
category count in memory on the verified 6.11.2 and 6.11.4 builds.
It restores the previous count or callback on unload; rebooting
without the extension restores normal access. Changes you make using the stock
controls follow the firmware's normal save behavior.

Activation checks the stock app fingerprint and memory mapping before
applying the change. Unsupported firmware is rejected without modifying it.

Build with `python3 dashborne/extensions/build.py`. Run the host checks with
`extensions/development-menu/test.sh`.
