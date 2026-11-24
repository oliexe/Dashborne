# Native extensions

Dashborne loads selected ARM shared libraries from microSD inside the official
UI process. Extensions can add controls and activities, use existing app
services, implement behavior hooks and workers, and replace an entire theme.
The SDK provides lifecycle callbacks, symbol access, startup activity selection
and managed resource overlays.

Dashborne activates extensions when you launch the dashboard or let the splash
expire, using the installed Linux system and app. Opening configuration does not
load extension code. A newly installed user extension is off until selected in
the patch list. Extra Menu and Widget Runtime are built into Dashborne and
start before selected extensions.

## Extension directory

See the bundled [Contrast](../../extensions/contrast) and
[Screen Mirroring](../../extensions/screen-mirroring) implementations.
Each source extension has a directory under `extensions/`:

```text
extensions/my-extension/
  manifest.json
  extension.c
  assets/                 # optional files supplied by the extension
```

```json
{
  "id": "my-extension",
  "title": "My extension",
  "description": "A short description of its behavior",
  "abi": 1,
  "sources": ["extension.c"]
}
```

The Dashborne build compiles these sources and installs the result as
`Dashborne/extensions/my-extension/libextension.so`, together with the manifest
and assets. It adds a native entry to the shared patch catalog. Set
`tweak.my-extension.enabled=1` in root `dashborne.conf` to enable it. Installation
preserves existing selections.
The catalog determines activation order. IDs must be unique across both types.

Bundled modules can reuse another extension's source assets without duplicating
them in Git. Add `"asset_sources": [{"extension": "tesla-ui", "paths": ["motion",
"Model3-CREDITS.md"]}]` to the manifest. The builder copies those files from the
named extension's `assets/` directory into the new package's own `assets/` tree;
the provider module does not need to be enabled. Paths cannot leave that source
directory, and duplicate destinations fail the build.

The supplied builder handles freestanding C sources. Other languages, C++,
additional dependencies or custom build systems can produce the same shared
library ABI: ARMv7 Linux, little-endian, hard-float, with dependencies compatible
with the installed system. Place those builds and their assets in the same SD
directory and add a catalog line with the fourth field `native`.

## Lifecycle and host API

Include [`dashborne_extension.h`](dashborne_extension.h) and export
`dashborne_extension_v1()`. It returns a persistent descriptor with ABI version,
structure size, matching extension ID, `activate(host)` and `deactivate()`.
Both lifecycle callbacks return zero on success.

`activate` runs on the UI thread after the loader prepares selected layout
patches and before it opens the official startup activity. The official library
has initialized its services by then. Initialize your extension in this callback
and keep long-running work in a worker that the extension owns. ELF constructors
run outside this lifecycle.

The host provides:

| Member | Purpose |
| --- | --- |
| `extension_directory` | Canonical directory for this extension's files. |
| `official_library`, `ui_runtime` | Loaded handles for the official app and FlyThings runtime. |
| `resolve(handle, name)` | Resolve exported symbols with `dlsym`; adapters can call services, register activities or implement hooks. |
| `log(host, message)` | Record an extension event in `Dashborne/loader.log`. |
| `overlay_file(host, source, target)` | During activation, bind a file relative to the extension directory over an existing absolute `/res/` file. The resulting mount is read-only. |
| `register_activity(host, name, create)` | During activation, register a native activity under a `dashborneExtension_` name through a resident host trampoline. It becomes inert before the plugin unloads. |
| `owns_callback(host, pointer)` | Check whether a callback belongs to a currently loaded extension before chaining a cooperating hook. |
| `register_extra(host, id, title, description, open)` | During activation, add an entry to the Extra menu; the host copies labels and retires callbacks before unloading. |
| `extra_count(host)`, `extra_entry(host, index, out)` | Read the active menu entries on the UI thread. |
| `open_extra(host, token)` | Invoke a registered action on the UI thread using its stable token. |
| `set_startup_activity(host, name)` | During activation, choose an activity registered by the extension instead of the official startup activity. Only one selected extension can claim this. |

An extension can supply its own layouts, icons, fonts and other theme resources,
or generate files from the installed resources during activation. Layouts can
reference new resources on SD. Use `resolve` to access the installed runtime and
implement version-specific symbol signatures and hooks in your extension. The
loader has no JavaScript/Lua interpreter; native extensions can embed one.

The host structure stays valid until cleanup. Call `overlay_file`,
`set_startup_activity` and `register_activity` on the activation thread. Destroy
registered activities and finish queued opens before reporting successful
cleanup. Factory names stay reserved to their original extension across
reactivation; the host supports 32 names per process. Dispatch UI calls from
workers through the runtime's UI-thread mechanism. The extension must stop and
join its workers, unregister callbacks, release controls and remove hooks in
`deactivate`, including when `activate` only partially succeeded. The host may
retry cleanup after a reported failure. The engine unloads a library only after
its cleanup succeeds.

## Extra menu entries

Use [`register_extra`](../core/extra-menu/README.md) for extension
pages that belong outside stock Settings. The bottom-right star icon opens
a Settings-style list. Register a title, description and callback after preparing
your window. Closing the window returns to Extra.

Entry IDs are local to an extension. The registry accepts up to 64 entries,
checks callback ownership and removes registrations during successful cleanup or
activation rollback. Menu tokens remain unique across reactivation. All registry
operations run on the UI thread; registrations are activation-only. New plugins
using these appended ABI-1 methods must check `struct_size` before accessing
them. Core services have separate lifecycle slots, leaving all 64 user-extension
slots available.

## Widget runtime

Use `dashborne_widget.h` to register widgets with the built-in runtime. The SDK
resolves `dashborne_widgets_v1` from the loaded `libdashborne.so`; no runtime
extension package is needed. Rebuild widgets made with the earlier SDK, which
looked for `extensions/widget-runtime/libextension.so`. The installer removes
the retired Extra Menu and Widget Runtime packages during an upgrade.

## Failure and reversal

All selected modules form one activation transaction. ABI mismatches, missing
entry points, activation failures, duplicate resource targets and competing
startup activities trigger cleanup in reverse order. A failed cleanup blocks
another launch in that process, preserving the library if its callbacks may
still be live. The configuration screen remains available with an error.

The engine records overlays in `Dashborne/extensions.mounts` before mounting. On
startup, it uses this journal to clear stale mounts before initializing the
official library. It removes managed mounts on shutdown or failed activation. A
selected layout patch and a native overlay cannot both own
`/res/ui/dash1act.ftu` in one session.

Booting without the card skips the loader and clears temporary mounts. This
reverses engine-managed resource changes. Native plugins execute in the UI
process with its privileges; Dashborne cannot undo their persistent writes or
external actions. Prefer the overlay helper for resource changes and implement
cleanup for runtime changes.

If a cleanup fails, reboot before retrying. If the log identifies a malformed
recovery journal, reboot to clear mounts, then remove or repair that journal on
the card before starting Dashborne again.

## Other patch formats

Store layout definitions in [`patches/definitions`](../patches/definitions).
DBP1 adds controls; interactive controls need a native extension to handle them.
DBP2 changes guarded, equal-length JSON scalar tokens in the dashboard layout.
Both compose against the same original file; mismatched fingerprints or
overlapping scalar edits reject the entire batch.
[`author.py`](../patches/author.py) produces metadata and offsets from layout bytes
read through ADB. Native overlays support larger structural or complete resource
replacements without the DBP2 scalar-size restriction.

## Validation

```sh
sh dashborne/extensions/test.sh
sh dashborne/patches/test.sh
sh dashborne/test.sh
DASHBORNE_CC=/opt/homebrew/opt/llvm/bin/clang \
DASHBORNE_LD=/opt/homebrew/opt/lld/bin/ld.lld \
sh dashborne/build.sh
```

Host tests load compiled test modules and mock privileged mount calls. They cover
lifecycle ordering, partial activation rollback, incompatible ABI/IDs/entry points,
custom activities, resource collisions, path checks, read-only remount failure,
busy cleanup and journal recovery. Layout tests cover combined edits, corruption,
bounds and conflicts. Run `sh test.sh` from the repository root for all core and
bundled-extension checks.
