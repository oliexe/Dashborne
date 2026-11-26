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
directories, with shared road, lamp and direction arrow images in `home`. Gray
is selected first; `home` supplies the white variant. Each frame is copied
unchanged. Re-run the importer to add direction arrows to an older model pack.

To add another body type with the same frame layout, import it into a separate
directory with `--model model-id`. Copy its variant folders into the installed
`models` directory and append their names to `models/index`, one per line.
The first line must remain `tesla-model-v1`; up to 16 variants are supported.

## Vehicle state

Park uses the front three-quarter view. Drive rotates through the overhead view
to a rear view; Reverse uses the overhead view with reversed road motion.
The road stops when reported speed is zero. In Drive or Reverse, a
cropped overhead view shows the front of the car at the bottom in Drive, or
the rear at the top in Reverse, with a moving direction arrow. The car eases
between this close-up and its normal driving view. The arrow disappears as
soon as speed rises, a panel opens, the gear changes to Park or Neutral, or the
Commander disconnects. Opening a panel brings the camera
back to the parked view, where each door, frunk and trunk animates independently.
Camera and panel motion ease into and out of each transition. Changing gear
mid-turn reverses from the current pose; the camera waits for panels to close
before leaving the parked view. Arrow motion eases at both ends of its loop;
zoom and translation use elapsed time between display callbacks. Road marks use a continuous projected pattern, with no road-frame PNG decoding.

The adapter reads the stock application's Commander state without changing its
subscriptions. It uses the previously verified connection, gear, door and turn
indicator mappings for the supported app build. Low and high beams use the same
fields as the stock light controls, checked against the app retrieved over ADB.
Blind-spot states color the lane boundaries independently of the turn lamps.
Only stock G/R turn-signal states illuminate the model; O is a side warning.
The current Commander field set exposes no verified steering-angle value, so
road curvature is not inferred from turn signals.
When disconnected, live indicators clear; the car stays at full opacity.
The stock widget frame supplies the rounded corners and edit outline. The
renderer covers its embedded navigation placeholder with a blank backing that
follows the active light or dark theme.
The full-tile touch target has no background fill, so the stock page's dimming
cannot reveal a rectangle over the car.

## Development

`scene.c` handles animation and layer placement without UI dependencies.
`data.c` reads vehicle state; `extension.c` connects the scene to native widget
controls. The renderer keeps model images in a bounded shared cache and releases them on
pause. Lane marks are antialiased in one native clipped drawing callback,
including during page slides, instead of issuing a native draw call per pixel. Visible widgets update
on the native 16 ms cadence; delayed callbacks advance elapsed time without
replaying missed frames. Zoom geometry remains continuous between sprite frames,
and the overhead view shares the close-up's image and position to avoid a jump.
On DASH, moving Drive and side-warning samples presented at about 59.7–59.8 fps,
with approximately 3.3 ms average widget drawing time. An occasional 80–100 ms
presentation gap remains; this is not a guarantee of uninterrupted 60 fps.
Actual frame rate still depends on display scheduling and image-loading cost.
Lamp anchors use source-image coordinates. Cropped black/red driving sprites
are placed in the same reference frame as the gray/white/blue sprites.

Run `sh extensions/tesla-model/test.sh` for the state-machine, parser, telemetry
and lifecycle tests. They cover all panel combinations, gear changes during
animation, cropped gear views, direction cues, light/dark backing, missing
images, model cycling, page reuse and bounded cache cleanup.
Device checks captured panel animations, Drive/Reverse views, lights and color
changes, including a color change through the native touch handler. The final
build completed 60 alternating page changes in one UI process, then returned to
configuration with the saved layout unchanged. These checks used sample states;
live Commander changes still need a vehicle check.
