# Tesla Model Widget

A 2×2 vehicle animation in the stock widget picker, under **Special → Gauges**.
The tile contains the car and driving animation, with no title or buttons.
Tap the car to cycle through the installed models and colors. The selection is
shared by its placements and survives page changes until the dashboard restarts.

## Install a model pack

Build Dashborne, then prepare the animation files separately:

```sh
python3 extensions/tesla-model/import_model.py /path/to/animation-ui /path/to/models
```

Copy the resulting `models` directory into
`Dashborne/extensions/tesla-model/` on the microSD. Enable **Tesla Model Widget**
in the boot menu and add it through the dashboard's layout editor.

The importer expects the `gray`, `home`, `black`, `blue` and `red` animation
directories, with shared road and lamp images in `home`. Gray is selected first;
`home` supplies the white variant. Each frame is copied unchanged.

To add another body type with the same frame layout, import it into a separate
directory with `--model model-id`. Copy its variant folders into the installed
`models` directory and append their names to `models/index`, one per line.
The first line must remain `tesla-model-v1`; up to 16 variants are supported.

## Vehicle state

Park uses the front three-quarter view. Drive rotates through the overhead view
to a rear view; Reverse uses the overhead view with reversed road motion.
The road stops when reported speed is zero. Opening a panel brings the camera
back to the parked view, where each door, frunk and trunk animates independently.

The adapter reads the stock application's Commander state without changing its
subscriptions. It uses the previously verified connection, gear, door and turn
indicator mappings for the supported app build. Low and high beams use the same
fields as the stock light controls, checked against the app retrieved over ADB.
When disconnected, live indicators clear; the car stays at full opacity.
The stock widget frame supplies the background and rounded corners.

## Development

`scene.c` handles animation and layer placement without UI dependencies.
`data.c` reads vehicle state; `extension.c` connects the scene to native widget
controls. The renderer shares a 12-image cache across placements, releases it on
pause, and requests updates at most 40 times per second while visible.

Run `sh extensions/tesla-model/test.sh` for the state-machine, parser, telemetry
and lifecycle tests. They cover all panel combinations, gear changes during
animation, missing images, model cycling, page reuse and bounded cache cleanup.
A timed test on the DASH captured panel animations, Drive/Reverse views, lights
and color changes, then restored configuration without changing the saved layout.
Touch gestures and live vehicle-state changes still need a hands-on check.
