# Changelog

This changelog tracks Dashborne core, starting with 0.1.0. Extension changes are
tracked separately in their source history.

## Unreleased

- Optional Force ADB keeps developer Wi-Fi connected while the stock dashboard
  runs. Auto recovery captures diagnostics and reboots into the boot menu after
  a crash or a stopped UI event loop.

- Custom widgets show size-specific picker previews, with optional artwork from
  extensions and a default preview for older widgets.

- Extra Menu uses a transparent Dashborne icon that appears on touch and hides
  after four seconds of inactivity.

- Extra Menu and Widget Runtime are built into Dashborne, with automatic removal
  of their old extension packages during upgrades.

- Shared widget registration API, native size/category support and card layout ownership.

- The extension builder can package shared source assets into independent modules.

## 0.1.0 — 2026-11-24

Initial release. This is the baseline for future Dashborne releases.

- microSD startup with a three-second splash and touchscreen boot menu.
- Temporary patch loading and a native extension engine for the installed UI.
- Optional Wi-Fi ADB with connection status; disabled by default.
- App browser with manual loading and optional autolaunch.
- Device information, including software versions and serial numbers.
- Unified card-root `dashborne.conf` for Wi-Fi, tweaks and loader preferences.
- Loading errors shown in the boot menu so failed modules can be disabled.
- Card installer that preserves settings and the previous startup configuration.
- Automated builds, tests and downloadable microSD release packages.
