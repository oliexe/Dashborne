# Tesla UI

A fixed instrument view in **Extra**, based on the current Model S cluster's
dark layout. Enable **Tesla UI** in Dashborne's Patches page, continue booting,
then open **Star → Tesla UI → Open**. The lower-right arrow returns to Extra.

Speed sits to the right of the vehicle graphic. Range, temperature, gear and
time occupy the bottom row. Park and charging use a larger Model 3 Highland
render; Drive, Reverse and Neutral use the rear view. The camera turns and
changes distance when shifting between Park and Drive, using perspective
renders in a fixed canvas. The layout has no customization controls.

Tap **Preview** at the upper right to begin in Park and shift into Drive after
two seconds. It then cycles through sample states, six seconds per scene: each
door and hatch, acceleration, regeneration, turn signals, blind-spot warnings,
hazards, reverse, charging and disconnection. An amber PREVIEW label names the
current scene. **Stop preview** returns to live readings and clears the sample
history. Leaving the view also ends preview; it starts disabled each time you
open Tesla UI. Sample values stay inside this view.

Tesla UI holds off automatic display sleep while the view is open. Stopping
Preview keeps that hold and returns to live data. Back, pause and destruction
release the hold; resuming the view acquires it again.

The controller-navigation handler is prepared locally: previous/next highlights
Back, select activates it, and Back returns to Extra. The native Activity Back
callback uses this handler; Knob and steering-wheel events still need an ADB
capture and adapter. See [controller input](../../docs/controller-input.md).

The left panel follows the Model S energy graph: an amber power trace, green
regeneration, a fading fill and a dashed average line. The average and time
span sit beneath the plot. It records one reading per second over 60 seconds
while this view is visible; missing intervals remain gaps. Its fixed scale is
+100/−50 kW. This is a power history, not Tesla's distance-based Wh/km graph.
The lower power meter uses +400/−100 kW and appears while driving. Both
displays clip at their bounds; the numeric average retains its value.

Navigation shows remaining distance, travel time and estimated arrival charge
when the stock data source supplies them. The energy panel and an illustrative
road surround the pre-rendered Model 3 Highland views. These images have poses
for all combinations of the four doors, frunk and trunk. Individual panel
changes animate to fixed hinge angles; the images do not represent measured
opening angles. The all-open pose also animates. Other combinations use their
endpoint poses. Turn signals use the existing arrows.

The body-state renderer is ready locally, but the live adapter does not yet
read those six panel states. Until their mapping is verified over ADB, the view
shows **Door status unavailable** with the neutral car image. Disconnects clear
body state. Headlights, brake lights, locks and charge-port state are not
mapped yet.

## Road scene

The perspective road appears in Drive, Reverse and Neutral. Markings move with
reported speed, including conversion from mph, and flow backward in Reverse.
They stop at zero or unavailable speed and in Neutral, where the direction is
unknown. Park, charging and disconnection hide the road. Timer stalls do not
cause a catch-up jump.

The existing turn-signal readings light the corresponding road edge green.
Known blind-spot occupancy produces an amber side zone and warning icon; an
active same-side turn signal or an urgent blind-spot flag makes it red. Left
and right availability are separate, and disconnects clear both. Road geometry
is illustrative: it does not represent detected lanes, steering angle or nearby
vehicle positions.

**Blind-spot input is not mapped yet.** The renderer and fixtures cover these
states, but the live adapter leaves them unavailable until their fields and
availability checks are verified over ADB. Turn signals do not imply blind-spot
occupancy. Only speed, gear and turn signals currently drive the live road
scene.

## Data and lifecycle

The current adapter supports the installed **6.11.2** app. Compatibility checks
run before registering the view. It reads the stock instrument getters, power
data, navigation scalars and turn-signal state at 10 Hz. Speed and speed limit
retain the stock displayed values; range, temperature, unit labels and clock
format follow the official settings. Power stays in kW.

The adapter uses the stock connection state. Disconnects clear the readings and
history; out-of-range values display a dash or hide the affected element. Stock
demo mode displays a waiting state. There is no independent per-signal
freshness timestamp. Live update cadence, reconnection and charging readings
still need hardware validation.

Opening this view keeps the existing data subscriptions. It does not send
vehicle commands or change Wi-Fi, projection or saved official settings. Pause
stops its timer; returning starts a new history. Pending opens and live
Activities prevent extension unloading until their callbacks are released.

## Rendering performance

The view requests a 16 ms animation timer for a 60 Hz display. Telemetry is
sampled every 100 ms and the history changes once per second. The power meter
interpolates between readings over 100 ms; speed numerals use the reported
integer directly. Disconnects, invalid power and mode changes cancel the
interpolation. Delayed callbacks skip missed samples instead of replaying them.

Control pointers and displayed properties are cached. Unchanged values produce
no native setters, and a speed change replaces only the affected digits. The
car canvas preloads the camera frames and advances by elapsed time. A gear
change during an orbit reverses from the current camera position. It draws
cropped sprites at their original size, without moving or resizing native
controls. Camera and hinge frames preload when the view opens. Only a changed
endpoint pose needs an image load during operation. The graph combines adjacent
identical columns without changing its pixels: a flat nonzero trace needs two
visible controls, compared with 828 separate columns. Steep, varying traces can
still use the full pool; unused controls stay hidden.

Road markings draw inside one fixed native control. Animation does not move or
resize controls, and at most one road repaint can be pending. A slow renderer
uses the latest phase instead of processing a backlog. Repaint requests are
limited to one per 16,667 microseconds; the native timer's actual cadence may
be slower. Warning textures change only with warning state. A stationary scene
produces no repeated native updates.

On Back or pause, the extension log records callback average/max duration,
callbacks exceeding 16,667 microseconds, scheduling gaps over 25 ms, source-
read time and setter/image-change counts. `startup_us` covers control-cache
preparation and the first update, after native layout creation. Logging happens
outside the animation loop. These measurements include synchronous native
setters, but not deferred painting or display scanout.

Sustained 60 fps on the DASH remains unverified. Device testing needs to cover
dense history, speed changes, cold car-image loads, open/back cycles and stock
projection running in the background. Callback timing can expose stalls; actual
presented-frame timing still needs a device-side capture.

## Development

The normal Dashborne build includes the generated layout and assets. To
regenerate:

```sh
uv run --with pillow --with resvg-py==0.5.0 extensions/tesla-ui/generate.py --assets
uv run --with pillow extensions/tesla-ui/typography.py
sh extensions/tesla-ui/test.sh
uv run --with pillow extensions/tesla-ui/preview.py
uv run --with pillow extensions/tesla-ui/preview.py --motion
sh extensions/tesla-ui/benchmark.sh
```

The car uses the [credited Highland mesh](assets/Model3-CREDITS.md). To
regenerate its perspective camera and hinge frames with Blender 5.2.3:

```sh
blender -b --python extensions/tesla-ui/render_vehicle.py -- /path/to/2024_tesla_model_3.glb --motion --samples 24 --output dashborne/build/vehicle-motion
uv run --with pillow extensions/tesla-ui/pack_motion.py dashborne/build/vehicle-motion
```

The renderer verifies the source hash. Packing retains each frame's position in
a 600 × 360 canvas and removes transparent margins. The 65 camera frames occupy
13,205,728 bytes of decoded pixels. Camera and hinge frames together use about
57.9 MiB; the text atlases add about 5 MiB. Six panel bits select the endpoint
poses: front left, rear left, front right, rear right, frunk and trunk.

Previews in `dashborne/build/tesla-ui-previews/` use test fixtures passed
through the native window renderer. They cover Drive, Park, Charging,
regeneration and disconnection, open panels and unknown body state. Cluster
labels use Montserrat Regular and speed numerals use Medium on both the DASH
and the host preview. The native adapter draws glyph atlases; it does not
change the official font registry. Road previews cover turn signals, hazards,
amber occupancy and red warnings. `--motion` produces `road-motion.webp` from a
synthetic sequence; its blind-spot values come only from the test fixture.

Tests cover units, signed power, invalid values, connection loss, history gaps,
state transitions, unknown panel states, graph interpolation and gaps, bounds,
pixel-equivalent graph merging, animation deadlines, stalled callbacks and
timer cleanup, speed-driven road motion, reverse flow and blind-spot
availability. Preview tests stop in every sample scene and interrupt all 64
panel masks. Lifecycle tests cover repeated toggles, late callbacks and
reentrant cleanup. A stationary 10-second fixture checks 625 timer callbacks, 100
data reads and zero native setters or lookups. The benchmark measures the host
C update path with fake telemetry and native setters; it excludes image
decoding and native painting, so its numbers are not device frame times. A
device capture exercised a full Preview cycle and returned to Tweaks. The test
recorded scheduling gaps, so it does not establish sustained 60 fps.

## References

- [Model S instrument panel, Tesla Owner's Manual](https://www.tesla.com/ownersmanual/models/en_us/GUID-29A7E205-A689-41D1-B69C-3AE821CB70E7.html): layout, Park/Drive behavior and power-meter direction.
- [Model S cluster photograph](https://imagecdn.zigwheels.ae/large/gallery/interior/95/1805/tesla-model-s-tachometer-469850.jpg): dark-theme spacing and speed placement.
- [Montserrat](https://github.com/google/fonts/tree/main/ofl/montserrat): cluster typeface, distributed under the [SIL Open Font License](assets/Montserrat-LICENSE.txt).
- [Layout and animation study](REFERENCE.md): reference geometry, video frames and remaining differences.
- [Model 3 Highland mesh and render credits](assets/Model3-CREDITS.md): RBLXSupercars, CC BY 4.0.
- [Model S energy graph photograph](https://www.platinumautohaus.com/2016-tesla-model-s-90d-c-13038.htm): amber trace, shaded area and average-line styling.
- [Enhance blind-spot display behavior](https://www.enhauto.com/blogs/all/update-4-8-1): yellow occupancy indication and red warning with a turn signal or proximity alert. This public description does not establish the installed DASH's data layout.
