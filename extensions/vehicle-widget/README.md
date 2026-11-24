# 3D Vehicle Widget

An experimental car widget injected into the official dashboard. It uses native
controls in the existing dashboard activity, with no separate page to open.
The standalone Tesla UI extension can stay disabled.

## Try it

1. Leave an empty **2×2 area** on a dashboard page: two columns across both rows.
2. Install the current build and enable **3D Vehicle Widget** in Dashborne.
3. Continue booting and open that dashboard page. The car appears in a free area,
   preferring the right side. If the page is full, the widget stays hidden.
4. Tap **Preview** to cycle through sample gears, doors, frunk and trunk states.
   **Stop** returns to live readings. Samples never enter the stock data buffers.

This test places a native control in unused dashboard space; it does not yet
add a selectable type to the stock widget editor. It leaves the saved layout
alone and hides during layout editing and page transitions. Removing Dashborne
or disabling the extension removes the injected widget on the next boot.

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
Hidden and paused widgets release their images. Shutdown restores the native
draw callback, removes listeners and timers, and restores the dashboard hooks.
The widget does not change display-sleep or networking policy.

## Local checks

```sh
sh extensions/vehicle-widget/test.sh
sh dashborne/build.sh
sh test.sh
```

Tests cover all 128 Park/Drive panel poses, cache bounds, partial asset failures,
invalid state, repeated Preview/Stop, editing, page movement, pause/resume,
destruction and hook restoration conflicts. The build copies shared car assets
into this extension's package, so it can run without Tesla UI's module loaded.
