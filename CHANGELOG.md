# Changelog

This changelog tracks Dashborne core, starting with 0.1.0. Extension changes are
tracked separately in their source history.

## Unreleased

- Preserve separately installed Tesla Model Widget packs during boot-menu updates
  and rollback, so the widget continues to load after an update.
- Allow custom widgets to load alongside Better Display Controls while preserving
  both extensions' dashboard cleanup callbacks.

- Limit updater Wi-Fi connection waits to 60 seconds, allow cancellation and show
  connection failures immediately without interrupting an existing ADB connection.

- Combine persistent ADB and automatic crash recovery into Developer Mode, with
  a confirmation dialog explaining Wi-Fi behavior and recovery.

- Show update progress in a modal with animated loading dots and separate status
  messages for verification, file preservation and microSD writes.
- Rename the release check to Check Updates and remove the duplicate version label.

## 0.1.2 — 2026-11-25

Target stock OS: 6.11.4.

- Update from the boot menu over Wi-Fi, with release selection, checksum and
  stock OS checks, interrupted-install recovery and rollback. Settings and
  community extensions are preserved; connecting for updates does not enable ADB.
- Clarify the Update Dashborne page with a Wi-Fi selector, installed version, release list
  and explicit install/restart confirmation.
- Build the updater in C with the existing Clang toolchain; no Go runtime or SDK.

- Configure developer Wi-Fi from the ADB page with an on-screen keyboard.
- Read the boot-menu version label from the compiled `VERSION` value.

## 0.1.1 — 2026-11-25

Target stock OS: 6.11.4.

- Support stock OS 6.11.4 in the core widget runtime and Force ADB adapter.

- Failed extensions are disabled and skipped with a warning, allowing boot to
  continue. Core-service failures fall back to the stock dashboard.

- Extra creates its entry as a runtime overlay instead of patching the stock
  dashboard layout, reducing its dependence on stock app updates.

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
