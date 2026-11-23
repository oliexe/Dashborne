# Extra menu

Dashborne's core menu adds **Extra** at the bottom right of the official
dashboard. It opens a separate Settings-style screen with a scrollable list
on the left and the selected extension's description and **Open** button on
the right. **Back** returns to the dashboard. Extension windows return to Extra
when closed.

The core loads automatically before selected plugins. It has no Patches toggle.
Only enabled plugins register entries; an empty menu explains how to add them.
Contrast remains in stock Settings as **\* Contrast**. Screen Mirroring lives
in Extra and no longer inserts a stock Settings category.

## Add an entry

During your extension's `activate`, after preparing its window:

```c
if (!host->register_extra ||
    host->register_extra(host, "viewer", "My extension",
                         "A short description of this page.", open_my_window))
    return -1;
```

The callback runs on the UI thread. Use the extension's own callback; entry IDs
are unique within that extension. Titles allow 63 UTF-8 bytes, descriptions 127,
and up to 64 entries are supported across enabled plugins. Entries are ordered
by registration. The host copies labels and removes callbacks before unloading
the owner, including when activation rolls back. Tokens are never reused in a
process, so a stale selection cannot open a different plugin.

## Implementation

`dashboard.json` is a guarded additive patch for the installed 6.11.2 dashboard.
The loader composes its button with selected layout patches, then loads the
core before user extensions. The core hooks only the dashboard click callback,
delegating every other control to the previous handler. App/runtime fingerprints
and the expected callback are checked before installation.

The menu uses a native Activity with a resident factory trampoline, an original
SD layout and the runtime's scrollable list. The host registry is independent
of the menu Activity and checks registration ownership and lifecycle. A live or
queued menu prevents its module from unloading. Dashboard hooks are restored
in reverse order. These changes last for the Dashborne session.

```sh
python3 extensions/extra-menu/generate.py
sh extensions/extra-menu/test.sh
sh app/extensions/test.sh
sh app/dashborne/test.sh
DASHBORNE_CC=/opt/homebrew/opt/llvm/bin/clang \
DASHBORNE_LD=/opt/homebrew/opt/lld/bin/ld.lld sh app/dashborne/build.sh
```

Host tests cover empty and 64-entry menus, selection and return, duplicate opens,
cleanup while a window is live, stale tokens, rollback, owner namespaces and
composition with selected patches. The dashboard patch also applies to the
private installed reference with all original controls preserved. Device
navigation and the Screen Mirroring round trip still need hardware confirmation.
The loader, core, updated plugins and catalog were installed with byte-for-byte
read-back verification. Existing settings were preserved, and configuration and
ADB returned after restarting the loader.
