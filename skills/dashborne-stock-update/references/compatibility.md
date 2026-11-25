# Compatibility audit

Start with `git status`, `VERSION`, `STOCK_OS_VERSION`, current manifests and build/test scripts. Do not depend on a fixed list of extensions. The inventory helper records file sizes, SHA-256 and CRC32; use ELF tools to inspect architecture, exported/imported symbols and disassembly where hooks changed.

## Core integration points

| Component | Source to inspect | Evidence required |
| --- | --- | --- |
| Boot menu and handoff | `dashborne/loader.c`, `state.c`, `extensions/engine.c` | FlyThings symbol/vtable ABI, stock activity handoff, failure ownership, teardown and retry behavior |
| Stock build identification | `dashborne/extensions/stock_compat.h` | Exact app fingerprint and build dispatch, including file-length assumptions and cached detection |
| ADB / Force ADB | `dashborne/debug.c`, `debug_force.c`, `watchdog.c` | Network and maintenance method signatures, radio reset call sites, daemon ownership, heartbeat and recovery |
| Extra overlay | `dashborne/core/extra-menu/` | Runtime ABI, activity lifecycle, callback ownership, touch forwarding, control IDs and cleanup |
| Widget Runtime | `dashborne/core/widget-runtime/` | Catalog registration, picker categories, native sizes, view parent/lifetime, edit outlines, drawing ownership and persistent IDs |

Extra no longer needs a stock dashboard layout patch, but still depends on the UI runtime and stock lifecycle hooks. Do not describe it as independent of all firmware changes. A rejected core module can affect boot with zero user extensions selected.

Version detection currently knows the 6.11.2 and 6.11.4 app builds. Read the current implementation rather than copying numbers from old notes. Audit every `DB_STOCK_OFFSET` caller and every `db_stock_version()` branch when adding a third version: the original two-way macro otherwise silently selects the old offset for another build. Search for hardcoded sizes, CRCs, instruction words, exported symbol names and offsets outside the shared header too, including version reporting in `dashborne/device.c`.

## Extensions

Read each manifest's sources and its probe/start/stop paths. Trace dependencies on core services. Current examples of required checks:

- Screen Mirroring: receiver fields, projection state, restart method calling convention (6.11.4 changed it), ownership of video layers, hotspot lifecycle and returning to stock CarPlay/AA.
- Tesla Model: Commander field mapping, pointer lifetimes, model assets, visibility, animation timers, multiple instances and teardown. Models absent from the release cannot be counted as live-tested.
- Tesla Skin: layout fingerprints, exported theme pointers and hook ownership. `prepare.py` emits original deltas from seven layouts; inspect its current CLI before running it. Supply `TESLA_SKIN_CAPTURE=/private/target-layouts` for its layout tests. Preserve stock activity names, control IDs, semantic indicators and callbacks.
- Development Menu: category array and count addresses, correct element width and cleanup. Similar nearby globals are not evidence of equivalence.
- Widget Example: validate through the core runtime rather than assuming it works because its own source has no offsets.

For every patch, check expected original bytes before modifying, bounds and alignment, all-or-nothing activation, and restoration after partial failure. Verify that a failed extension is disabled and boot continues with a warning; core failure must allow unmodified stock handoff. Config parse failures must not overwrite user settings. Do not ship diagnostic hooks or forced selections.

## Build and tests

Repository commands:

```sh
sh dashborne/build.sh
HOST_CC=clang CC=clang sh test.sh
python3 tools/release.py
```

On the established Mac, cross-compilation uses:

```sh
DASHBORNE_CC=/opt/homebrew/opt/llvm/bin/clang \
DASHBORNE_LD=/opt/homebrew/opt/lld/bin/ld.lld sh dashborne/build.sh
```

CI installs `clang`, `lld`, Python and zlib development files on Ubuntu. Host tests cannot prove pointer lifetimes, rendering or network behavior on the device. Record skipped capture-dependent tests explicitly. Source changed after a successful test requires the affected checks to run again.
