# 3D Vehicle Widget

Adds **3D Vehicle** to the official widget picker. The dashboard editor owns its
position, occupied cells and removal. The standalone Tesla UI extension can stay
disabled.

## Try it

1. Install the current build and enable **3D Vehicle Widget** in Dashborne.
2. Continue booting, enter the dashboard's layout editor and select an empty cell.
3. Open the special widgets' **Gauges** category and choose **3D Vehicle**.
4. Leave editing to use **Preview**. **Stop** returns to live vehicle readings.

Choose any of the eight stock sizes, from **1×1** to **6×2**. Multiple placements
can share a page when space permits. The shared widget runtime owns placement
and editing; the car extension supplies rendering and vehicle data.

Use **Preview** to cycle sample states and **Stop** to return to live readings.
Preview is hidden while editing. All car placements share the same preview and
image cache.

Layouts are stored on the card through the [widget runtime](../../dashborne/core/widget-runtime).
Existing 3D Vehicle placements retain their source ID. This adapter is guarded
for app 6.11.2 and its inspected UI runtime.

## Vehicle state

Park uses the larger perspective view; Drive, Reverse and Neutral use the rear
view. Shifting between them animates the camera. Doors, frunk and trunk use the
same six status fields as the stock action widgets. Disconnects and invalid
panel values clear the previous pose and show an unavailable status.

Individual panels and the all-open pose have hinge animations. Other panel
combinations use endpoint poses. The car is rendered from the existing
[credited Model 3 mesh](../tesla-ui/assets/Model3-CREDITS.md), using pre-rendered
frames rather than a live 3D engine.

## Rendering and lifecycle

The cache holds 65 camera frames, one body pose and at most one 11-frame hinge
sequence. Its decoded pixels total at most 19.54 MiB, excluding native bitmap
overhead. It loads no Tesla UI fonts, road graphics, graph or standalone activity.
Unchanged scenes do not request another repaint; only one repaint can be pending.

Vehicle data is sampled at 10 Hz, with animation ticks every 16 ms. The runtime
attaches each view to its moving page container and handles editing, clipping,
pause and teardown. All placements share one cache, which is released when the
last assigned car view is retired or the dashboard pauses. The widget does not change display-sleep or
networking policy.

## Local checks

```sh
sh extensions/vehicle-widget/test.sh
sh dashborne/core/widget-runtime/test.sh
sh dashborne/build.sh
sh test.sh
```

Scene tests cover panel poses, cache bounds, asset failures, invalid state and
repeated Preview/Stop. Adapter tests cover all sizes, multiple views and cache
release. Native picker and lifecycle checks live with the shared runtime.
