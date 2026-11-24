# Native extensions

Dashborne loads selected ARM shared libraries from microSD inside the official
UI process. Extensions can add controls and activities, use existing app
services, implement behavior hooks and workers, and replace an entire theme.
The SDK supplies lifecycle, symbol access, startup activity selection and managed
resource overlays. It does not limit native code to a list of visual operations.

The official Linux installation and app remain the base. Extensions activate
only after the user launches the dashboard or lets the splash expire. Opening
configuration does not load extension code. A newly installed user extension is off
until selected in the patch list. The bundled Extra menu core loads automatically
before selected extensions and is omitted from that list.

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

The normal Dashborne build compiles these sources and installs the result as
`Dashborne/extensions/my-extension/libextension.so`, together with the manifest
and assets. It adds a native entry to the shared patch catalog. Settings use
`tweak.my-extension.enabled=1` in root `dashborne.conf`, like layout patches; installation preserves choices.
The catalog determines activation order. IDs must be unique across both types.

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

`activate` runs on the UI thread after selected layout patches are prepared and
before the official startup activity is opened. Official library initialization
has already occurred so its services are available. Put initialization here,
not in ELF constructors. Return promptly; longer work belongs to a worker owned
by the extension.

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
| `register_extra(host, id, title, description, open)` | During activation, add an entry to the Extra menu; labels are copied and callbacks retire before unloading. |
| `extra_count(host)`, `extra_entry(host, index, out)` | Read the active menu entries on the UI thread. |
| `open_extra(host, token)` | Invoke a registered action on the UI thread using its stable token. |
| `set_startup_activity(host, name)` | During activation, choose an activity registered by the extension instead of the official startup activity. Only one selected extension can claim this. |

An extension can supply its own layouts, icons, fonts and other theme resources,
or generate files from the installed resources during activation. New resources
can live on the SD card and be referenced by those layouts. `resolve` provides
access to the actual runtime; it is not a portable widget toolkit or an automatic
hook generator. Version-dependent symbol signatures and hooks belong to the
extension. The loader does not run a JavaScript/Lua interpreter, but a native
extension can embed one.

The host structure stays valid until cleanup. `overlay_file` and
`set_startup_activity` and `register_activity` are activation-thread operations.
A registered activity must be destroyed, and any queued open completed, before
its plugin reports successful cleanup. Factory names stay reserved to their
original extension across reactivation; the host supports 32 names per process. UI calls from workers
must be marshalled through the runtime's UI-thread mechanism. The extension must
stop and join its workers, unregister callbacks, release controls and remove hooks
in `deactivate`, including when `activate` only partially succeeded. Cleanup may
be retried after a reported failure. The engine unloads a library only after its
cleanup succeeds.

## Extra menu entries

Use [`register_extra`](../../extensions/extra-menu/README.md) for extension
pages that belong outside stock Settings. The bottom-right Extra button opens
a Settings-style list. Register a title, description and callback after preparing
your window. Close that window normally to return to Extra.

Entry IDs are local to an extension. The registry accepts up to 64 entries,
checks callback ownership and removes registrations during successful cleanup
or activation rollback. Menu tokens remain unique across reactivation. All
registry operations run on the UI thread; registrations are activation-only.
New plugins using these appended ABI-1 methods must check `struct_size` before
accessing them. The bundled `extra-menu` core is packaged with `core: true`,
loads first, and uses a reserved slot in addition to 64 selected user extensions.

## Failure and reversal

All selected modules form one activation transaction. ABI mismatches, missing
entry points, activation failures, duplicate resource targets and competing
startup activities trigger cleanup in reverse order. A failed cleanup blocks
another launch in that process, preserving the library if its callbacks may
still be live. The configuration screen remains available with an error.

Managed overlays are recorded in `Dashborne/extensions.mounts` before mounting.
On loader startup, that journal clears stale file mounts from a previous UI
process before the official library initializes. On normal shutdown or failed
activation, managed mounts are removed. A selected layout patch and a native
overlay cannot both own `/res/ui/dash1act.ftu` in one session.

Booting without the card skips the loader and clears temporary mounts. This
reverses engine-managed resource changes. Native plugins execute in the UI
process with its privileges; their own persistent writes or external actions
cannot be automatically undone by Dashborne. Prefer the overlay helper for
resource changes and implement cleanup for runtime changes.

If a cleanup fails, reboot before retrying. If the log identifies a malformed
recovery journal, reboot to clear mounts, then remove or repair that journal on
the card before starting Dashborne again.

## Other patch formats

Layout definitions remain in [`patch definitions`](../patches/definitions).
DBP1 adds controls (interactive controls require an owning native extension); DBP2 changes guarded, equal-length JSON scalar
tokens in the inspected dashboard layout. Both compose against the same original
file; mismatched fingerprints or overlapping scalar edits reject the entire
batch. [`author.py`](../patches/author.py) produces metadata and offsets from a
local reference. Native overlays support larger structural or complete resource
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
