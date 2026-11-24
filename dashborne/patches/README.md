# Layout patches

Dashborne compiles JSON patch definitions into `.dbp` files and a catalog
for `Dashborne/patches` on microSD. The bundled [badge](definitions/badge.json)
adds the Dashborne label; users select it in the boot menu.

- DBP1: append a root control. Interactive controls need a native extension
  to handle their events.
- DBP2: replace guarded, equal-length JSON scalar tokens. Use
  [`author.py`](author.py) to calculate offsets and metadata from a local reference.
- For new activities, behavior or whole resource replacements, use the
  [native extension SDK](../extensions/README.md).

The engine verifies the installed layout's size and checksums before applying
any edits. The engine applies selected definitions to that original layout and rejects
the batch if edits conflict or fingerprints differ. Dashborne writes
the result to `Dashborne/generated.ftu` on the card and temporarily bind-mounts
it over the installed layout. Rebooting without the card restores the stock UI.

Build with `sh dashborne/build.sh` from the repository root. Test the compiler
and native patch engine with `sh dashborne/patches/test.sh`.
