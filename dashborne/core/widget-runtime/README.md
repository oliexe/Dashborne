# Widget runtime

The widget runtime is linked into `libdashborne.so` and starts before user
extensions. It owns picker registration, card layouts and custom widget
instances. Without a registered widget, it installs no hooks.

Each custom instance is attached directly to a stock page container and occupies
cells through the stock grid. Its content moves and clips with the page. Custom
instances use their own objects, leaving the stock gauge and projection pools
available. The projection lookup excludes custom wide widgets.

The adapter checks the installed app and UI runtime before changing memory.
Registration tables and code hooks are restored after dashboard teardown. A
conflicting hook or failed restoration prevents unloading. Layout reads and
writes are redirected to RAM, with saves stored in `dashborne.conf`; teardown
loads the flash layout before restoring its write path.

Start with [widget-example](../../../extensions/widget-example) to add a custom widget.

## Verification

The size mapping, category lists, descriptor layout, page containers and native
placement calls were inspected through ADB on app 6.11.2. Automated local checks
cover:

- All eight sizes in all 25 categories, plus invalid metadata and unavailable space.
- Stock picker delegation, replacement rollback and reopening a custom placement.
- Two providers with stable IDs across registration order and enable/disable.
- Three page parents, pooled view reassignment, edit-mode action gating and pause.
- Card-layout round trips, malformed configuration, failed writes and cleanup order.
- Hook conflicts, memory-protection failures and cache-flush failures.

Run `sh dashborne/core/widget-runtime/test.sh` and `sh test.sh`. The optional `DASHBORNE_WIDGET_DEVICE_TEST` build runs
`tests/device_probe.h` against native page objects; it does not save the temporary
placements. Normal release builds exclude that probe.

The hardware probe passed on app 6.11.2 with both frameworks linked into
`libdashborne.so` and their old extension directories removed:

- All eight sizes matched the native grid dimensions and control positions.
- Native button lookup and edit-mode action visibility passed at each size.
- Picker row counts matched the stock callbacks in all 25 categories, with all
  eight custom size choices found in each category.
- Multiple custom widgets coexisted with independent example state.
- Custom wide widgets were excluded from the native projection lookup.

These checks changed only live grid objects; they did not save temporary
placements. The normal build was restored afterward. Touch placement/removal,
rapid page swipes, theme changes, knob navigation and coexistence with active
projection still need confirmation on the final build before claiming complete
stock-widget parity.
