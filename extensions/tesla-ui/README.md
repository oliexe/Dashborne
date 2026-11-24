# Tesla UI

A fixed instrument view in **Extra**, based on the current Model S cluster's
dark layout. Enable **Tesla UI** in Dashborne's Patches page, continue booting,
then open **Star → Tesla UI → Open**. The lower-right arrow returns to Extra.

Speed sits to the right of the vehicle graphic. Range, temperature, gear and
time occupy the bottom row. Park and charging use a larger Model 3 Highland render;
Drive, Reverse and Neutral use the rear view. The layout has no customization
controls.

The left panel follows the Model S energy graph: an amber power trace, green
regeneration, a fading fill and a dashed average line. The average and time span
sit beneath the plot. It records one reading per second over 60 seconds while
this view is visible; missing intervals remain gaps. Its fixed scale is
+100/−50 kW. This is a power history, not Tesla's distance-based Wh/km graph.
The lower power meter uses +400/−100 kW and appears while driving. Both displays
clip at their bounds; the numeric power reading retains its value.

Navigation shows remaining distance, travel time and estimated arrival charge
when the stock data source supplies them. Maps and traffic/lane visualization
are replaced by the energy panel and pre-rendered Model 3 Highland views. These
images do not indicate doors, lights or nearby objects.

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

## Development

The normal Dashborne build includes the generated layout and assets. To regenerate:

```sh
uv run --with pillow --with resvg-py==0.5.0 extensions/tesla-ui/generate.py --assets
sh extensions/tesla-ui/test.sh
uv run --with pillow extensions/tesla-ui/preview.py
```

The car images are rendered from a 3D mesh in Blender; they are not regenerated
by `generate.py`. Download the [credited GLB](assets/Model3-CREDITS.md), then run
from the repository root (Blender 5.2.3):

```sh
blender -b --python extensions/tesla-ui/render_vehicle.py -- /path/to/2024_tesla_model_3.glb
```

The renderer verifies the source hash and produces both transparent PNGs at the
widget dimensions. The regular DASH build uses these images without a 3D runtime.

Previews in `dashborne/build/tesla-ui-previews/` use test fixtures passed through
the native window renderer. They cover Drive, Park, Charging, regeneration and
disconnection. Preview body text uses Inter; on the DASH, supporting labels use
the stock font. Large speed numerals use the bundled Inter glyph images on both.

Tests cover units, signed power, invalid values, connection loss, history gaps,
state transitions, graph interpolation and gaps, bounds, timer cleanup and repeated open/back actions. The
ARM package is built; installation and visual checks on the DASH are pending.

## References

- [Model S instrument panel, Tesla Owner's Manual](https://www.tesla.com/ownersmanual/models/en_us/GUID-29A7E205-A689-41D1-B69C-3AE821CB70E7.html): layout, Park/Drive behavior and power-meter direction.
- [Model S cluster photograph](https://imagecdn.zigwheels.ae/large/gallery/interior/95/1805/tesla-model-s-tachometer-469850.jpg): dark-theme spacing and speed placement.
- [Inter](https://github.com/google/fonts/tree/main/ofl/inter): numeral typeface, distributed under the [SIL Open Font License](assets/Inter-LICENSE.txt).
- [Model 3 Highland mesh and render credits](assets/Model3-CREDITS.md): RBLXSupercars, CC BY 4.0.
- [Model S energy graph photograph](https://www.platinumautohaus.com/2016-tesla-model-s-90d-c-13038.htm): amber trace, shaded area and average-line styling.
