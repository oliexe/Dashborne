# Roadmap

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
- [ ] Identify the active USB controller and gadget configuration interface.
- [ ] Verify USB enumeration and an ADB shell on the development laptop.

## Dashboard features

- [ ] Map the local communication service interface.
- [ ] Display live data through existing services.
- [ ] Choose and implement the first additional features.
- [ ] Test state handling, reconnects and UI behavior.
