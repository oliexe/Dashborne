# 3D Vehicle Widget

Adds **3D Vehicle** to the official widget picker. The dashboard editor owns its
position, occupied cells and removal. The standalone Tesla UI extension can stay
disabled.

## Try it

1. Install the current build and enable **3D Vehicle Widget** in Dashborne.
2. Continue booting, enter the dashboard's layout editor and select an empty cell.
3. Open the special widgets' **Gauges** category and choose **3D Vehicle**.
4. Leave editing to use **Preview**. **Stop** returns to live vehicle readings.

The widget needs a **2×2 area** and supports one car per page. It uses a free
native gauge container; the existing gauge choices remain available. If all four
containers are in use or no 2×2 area fits, the picker explains that it cannot be
placed. Use the stock editor to change or remove it. Preview is hidden while
editing, leaving the stock controls available.

The extension starts with a copy of your current dashboard layout. While it is
enabled, layout changes are saved in the root card's `dashborne.conf`, under
`vehicle-widget.layout.*`. Disabling the extension or booting without Dashborne
returns to the flash layout; enabling it again restores the card's layout.

This build has passed local checks. The picker, touch handling and placement
still need testing on a DASH.

## Vehicle state

Park uses the larger perspective view; Drive, Reverse and Neutral use the rear
view. Shifting between them animates the camera. Doors, frunk and trunk use the
same six status fields as the stock action widgets. Disconnects and invalid
panel values clear the previous pose and show an unavailable status.

Individual panels and the all-open pose have hinge animations. Other panel
combinations use endpoint poses. The car is rendered from the existing
[credited Model 3 mesh](../tesla-ui/assets/Model3-CREDITS.md), using pre-rendered
frames rather than a live 3D engine.

The adapter is guarded for the inspected 6.11.2 app and UI runtime. The field
mapping and injection path have been checked locally; live telemetry, native
placement, touch and frame rate still need a DASH test.

## Rendering and lifecycle

The cache holds 65 camera frames, one body pose and at most one 11-frame hinge
sequence. Its decoded pixels total at most 19.54 MiB, excluding native bitmap
overhead. It loads no Tesla UI fonts, road graphics, graph or standalone activity.
Unchanged scenes do not request another repaint; only one repaint can be pending.

Data and placement are sampled at 10 Hz, with animation ticks every 16 ms.
Hidden and paused widgets release their images. Cleanup restores the native
draw callback, picker tables, layout-path references and dashboard hooks after
the dashboard has closed. A live dashboard requires a restart before unloading.
The widget does not change display-sleep or networking policy.

## Local checks

```sh
sh extensions/vehicle-widget/test.sh
sh dashborne/build.sh
sh test.sh
```

Tests cover all 128 Park/Drive panel poses, cache bounds, partial asset failures,
invalid state, repeated Preview/Stop, picker selection, placement, replacement
rollback, re-editing, page movement, pause/resume, destruction and hook conflicts.
Storage tests cover round trips, malformed data, size limits and failed writes.
The build copies shared car assets into this extension's package, so it can run
without Tesla UI's module loaded.
