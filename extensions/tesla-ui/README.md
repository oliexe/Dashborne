# Tesla UI

A fixed instrument view in **Extra**, based on the current Model S cluster's
dark layout. Enable **Tesla UI** in Dashborne's Patches page, continue booting,
then open **Star → Tesla UI → Open**. The lower-right arrow returns to Extra.

Speed sits to the right of the vehicle graphic. Range, temperature, gear and
time occupy the bottom row. Park and charging use a larger Model 3 Highland render;
Drive, Reverse and Neutral use the rear view. The layout has no customization
controls.

The controller-navigation handler is prepared locally: previous/next highlights
Back, select activates it, and Back returns to Extra. The native Activity Back
callback uses this handler; Knob and steering-wheel events still need an ADB
capture and adapter. See [controller input](../../docs/controller-input.md).

The left panel follows the Model S energy graph: an amber power trace, green
regeneration, a fading fill and a dashed average line. The average and time span
sit beneath the plot. It records one reading per second over 60 seconds while
this view is visible; missing intervals remain gaps. Its fixed scale is
+100/−50 kW. This is a power history, not Tesla's distance-based Wh/km graph.
The lower power meter uses +400/−100 kW and appears while driving. Both displays
clip at their bounds; the numeric average retains its value.

Navigation shows remaining distance, travel time and estimated arrival charge
when the stock data source supplies them. Maps and traffic/lane visualization
are replaced by the energy panel and pre-rendered Model 3 Highland views. These
images have poses for all combinations of the four doors, frunk and trunk.
Open/closed flags select fixed hinge angles; the images do not represent measured
opening angles. Turn signals use the existing arrows.

The body-state renderer is ready locally, but the live adapter does not yet read
those six panel states. Until their mapping is verified over ADB, the view shows
**Door status unavailable** with the neutral car image. Disconnects clear body
state. Headlights, brake lights, locks and charge-port state are not mapped yet.

## Data and lifecycle

The current adapter supports the installed **6.11.2** app. Compatibility checks
run before registering the view. It reads the stock instrument getters, power
data, navigation scalars and turn-signal state at 10 Hz. Speed and speed limit
retain the stock displayed values; range, temperature, unit labels and clock
format follow the official settings. Power stays in kW.

The adapter uses the stock connection state. Disconnects clear the readings and
history; out-of-range values display a dash or hide the affected element.
Stock demo mode displays a waiting state. There is no independent per-signal
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
no native setters, and a speed change replaces only the affected digits. Car
images change only when the view or panel state changes. The graph combines
adjacent identical columns without changing its pixels: a flat nonzero trace
needs two visible controls, compared with 828 separate columns. Steep, varying
traces can still use the full pool; unused controls stay hidden.

On Back or pause, the extension log records callback average/max duration,
callbacks exceeding 16,667 microseconds, scheduling gaps over 25 ms, source-read
time and setter/image-change counts. `startup_us` covers control-cache preparation
and the first update, after native layout creation. Logging happens outside the
animation loop. These measurements include synchronous native setters, but not
deferred painting or display scanout.

Sustained 60 fps on the DASH remains unverified. Device testing needs to cover
dense history, speed changes, cold car-image loads, open/back cycles and stock
projection running in the background. Callback timing can expose stalls; actual
presented-frame timing still needs a device-side capture.

## Development

The normal Dashborne build includes the generated layout and assets. To regenerate:

```sh
uv run --with pillow --with resvg-py==0.5.0 extensions/tesla-ui/generate.py --assets
sh extensions/tesla-ui/test.sh
uv run --with pillow extensions/tesla-ui/preview.py
sh extensions/tesla-ui/benchmark.sh
```

The car images are rendered from a 3D mesh in Blender; they are not regenerated
by `generate.py`. Download the [credited GLB](assets/Model3-CREDITS.md), then run
from the repository root (Blender 5.2.3):

```sh
blender -b --python extensions/tesla-ui/render_vehicle.py -- /path/to/2024_tesla_model_3.glb
blender -b --python extensions/tesla-ui/render_vehicle.py -- /path/to/2024_tesla_model_3.glb --states all --samples 48 --output extensions/tesla-ui/assets/body
```

The renderer verifies the source hash and produces both transparent PNGs at the
widget dimensions. `--states all` renders 64 combinations for each camera;
`--states 4,16` renders only front-right-door and frunk poses. The bit order is
front left, rear left, front right, rear right, frunk, trunk, using vehicle-relative
left/right. The regular DASH build uses these images without a 3D runtime.

Previews in `dashborne/build/tesla-ui-previews/` use test fixtures passed through
the native window renderer. They cover Drive, Park, Charging, regeneration and
disconnection, open panels and unknown body state. Preview body text uses Inter;
on the DASH, supporting labels use the stock font. Large speed numerals use the bundled Inter glyph images on both.

Tests cover units, signed power, invalid values, connection loss, history gaps,
state transitions, unknown panel states, graph interpolation and gaps, bounds,
pixel-equivalent graph merging, animation deadlines, stalled callbacks and
timer cleanup. A steady 10-second fixture checks 625 timer callbacks, 100 data
reads and zero native setters or lookups. The benchmark measures the host C
update path with fake telemetry and native setters; it excludes image decoding
and native painting, so its numbers are not device frame times. The ARM package is built;
installation and visual checks on the DASH are pending.

## References

- [Model S instrument panel, Tesla Owner's Manual](https://www.tesla.com/ownersmanual/models/en_us/GUID-29A7E205-A689-41D1-B69C-3AE821CB70E7.html): layout, Park/Drive behavior and power-meter direction.
- [Model S cluster photograph](https://imagecdn.zigwheels.ae/large/gallery/interior/95/1805/tesla-model-s-tachometer-469850.jpg): dark-theme spacing and speed placement.
- [Inter](https://github.com/google/fonts/tree/main/ofl/inter): numeral typeface, distributed under the [SIL Open Font License](assets/Inter-LICENSE.txt).
- [Model 3 Highland mesh and render credits](assets/Model3-CREDITS.md): RBLXSupercars, CC BY 4.0.
- [Model S energy graph photograph](https://www.platinumautohaus.com/2016-tesla-model-s-90d-c-13038.htm): amber trace, shaded area and average-line styling.
