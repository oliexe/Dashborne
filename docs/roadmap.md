# Roadmap

## Dashborne startup and configuration

- [x] Combine the SD launcher, Wi-Fi ADB controls and patch injector.
- [x] Show the supplied icon and a three-second tap-to-configure splash.
- [x] Add persistent Wi-Fi ADB and patch selections on microSD.
- [x] Build a patch catalog from the repository definitions, with compatibility checks.
- [x] Compose multiple selected additive patches against the installed base layout.
- [x] Render configuration and live ADB status on the DASH.
- [x] Verify the automatic splash-to-official-dashboard transition on hardware.
- [x] Verify tapping the splash cancels automatic launch on hardware.
- [x] Verify normal startup from the persisted card selection after reboot.

## Official app patches

- [x] Forward the installed official app from an SD launcher and confirm rendering.
- [x] Build a separate additive layout patch and a native patch engine.
- [x] Generate patched resources on the device and override individual files.
- [x] Add version compatibility checks, preview recovery and explicit restoration.
- [x] Verify the badge renders within the official dashboard on hardware.
- [x] Verify reboot clears the override and restores the original layout byte-for-byte.
- [x] Verify the preview timer automatically reboots into the previous SD app.
- [ ] Keep Wi-Fi ADB reachable alongside the official dashboard.
- [ ] Verify a confirmed injector card reapplies the patch after reboot.
- [ ] Extend the patch format for further visual changes and behavior hooks.

## Completed

- [x] Identify the application package and platform.
- [x] Execute our library from microSD on the installed Linux system.
- [x] Inspect the launcher, startup configuration and runtime mappings.

## Minimal alternative screen

- [x] Map launcher callbacks and essential initialization.
- [x] Choose an integration with the existing framework and services.
- [x] Build an original screen with local lifecycle and fallback tests.
- [x] Render a custom screen and verify touch input.
- [ ] Verify startup, shutdown and return to the installed application.

## Debugging

- [x] Add USB ADB controls and live service status to the app.
- [x] Add read-only USB diagnostics after the legacy control path proved unavailable.
- [x] Identify the exposed USB device controller and installed ConfigFS helper.
- [x] Add a ConfigFS backend and tests for setup, endpoint waits and unbinding.
- [x] Connect through the installed Wi-Fi workflow and add network ADB controls.
- [x] Verify Wi-Fi ADB, root shell access and file transfers from the laptop.
- [x] Capture the live screen and deploy/restart an updated UI over Wi-Fi.
- [ ] Verify USB enumeration and a USB ADB shell on the development laptop.

## Development loader

- [x] Build a separate minimal ADB Enabler app.
- [x] Read Wi-Fi settings from the SD card and start TCP ADB automatically.
- [x] Show centered connection status and address.
- [x] Verify on-screen rendering and automatic recovery of a stopped ADB daemon.
- [x] Verify automatic Wi-Fi and ADB startup after a full device reboot.

## Instrument cluster mockup

- [x] Build a Tesla-inspired layout at the native 1920 × 480 resolution.
- [x] Add original car artwork, animated lanes, traffic and simulated speed.
- [x] Add navigation/energy, Park/Drive and pause/resume interactions.
- [x] Test the simulation and native control callbacks locally.
- [x] Deploy remotely and inspect the rendered screen on the DASH.
- [x] Verify all three interactions on the physical touchscreen.
- [x] Preserve ADB Enabler as the startup selection after preview deployment.

## Future extensions

- [ ] Map the local communication service interface.
- [ ] Extend the official app's live-data screens through existing services.
- [ ] Choose and implement the first additional features.
- [ ] Test state handling, reconnects and UI behavior.
