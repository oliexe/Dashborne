# Building themes

Tesla Skin includes the theme engine for stock OS **6.11.4**. Its
[`theme.json`](theme.json) defines the Tesla palette; the adapter handles native
resources, layout changes and light/dark switching.

## Theme definition

Edit `theme.json` to change the font, accent, positive-state color and both
palettes. `dash` means dark mode. Colors use `#RRGGBB`; the font name must match
the family registered by the TTF placed in `assets/`.

The `controls` object accepts a layout name followed by a control caption or
numeric ID. Use `*` for every control in that layout. For example:

```json
{
  "controls": {
    "awlsettings": {
      "TOPICS_LIST": {
        "position": {"left": 185, "width": 440}
      }
    },
    "dash1act": {
      "PT_G1_SPEED_VAL": {"fontSize": 104}
    }
  }
}
```

Overrides merge with existing positions and color tables. The compiler accepts
visual properties only: font, size, weight, colors, pictures and geometry.
Control IDs, callbacks and vehicle-data bindings stay native. Control color
tables use the native integer RGB representation.

## Images

Place an authored replacement at `assets/ui/<stock-relative-path>`. For example,
`assets/ui/dash/stats/navi/nav_dest_ico.png` replaces that dark-mode icon. Add a
matching `light/` variant for light mode. For paths without a stock theme prefix,
such as `Knob/`, use `assets/ui/dash/Knob/` and `assets/ui/light/Knob/`;
`assets/ui/Knob/` supplies a shared override. Resource lookup accepts any PNG under
the stock UI directory; it does not require a new C hook or a fixed route list.

The generator draws the resources listed in `resource-sizes.json`. Add new
generated shapes there and implement their drawing in `generate.py`, or add a
finished PNG directly. Keep the native dimensions for sprite strips, cropped
images and state indicators.

When no authored replacement exists, the engine recolors neutral pixels from
the installed stock image using the selected palette. It preserves alpha,
geometry and saturated colors such as warning indicators. Generated images are
cached in `/tmp` for that UI process; palette conversion does not run per frame.
Unreadable or unsupported images fall back to the original resource.

## Compile and check

Capture resources from the connected device into a local working directory:

```sh
adb pull /res/ui /tmp/dash-ui-6.11.4
python3 extensions/tesla-skin/generate.py
python3 extensions/tesla-skin/prepare.py /tmp/dash-ui-6.11.4
TESLA_SKIN_CAPTURE=/tmp/dash-ui-6.11.4 sh extensions/tesla-skin/test.sh
python3 extensions/tesla-skin/audit.py /tmp/dash-ui-6.11.4 \
  --output /tmp/theme-coverage.json
python3 dashborne/extensions/build.py
```

The image generator requires Pillow. To compile another definition, set
`DASHBORNE_THEME=/absolute/path/to/theme.json` for **both** `generate.py` and
`prepare.py`. The resulting extension contains the selected theme. Rebuild and
restart after changing it; only one theme definition is active at a time.

The audit checks each captured layout and runs every DASH PNG through the image
adapter, including the Knob overlay sprites. Audio configuration is outside the theme engine.
An asset coverage pass does not replace an on-device visual check.

## Device validation

Use the stock Development → Production mode for sample data. Check both display
themes, dashboard pages, the three picker categories, widget sizes, page editing,
Settings tabs, dialogs and the keyboard. Include long labels and non-Latin media
or navigation text. Leave Production mode disabled after the check.

Native unit glyphs retain their stock font, and incoming CarPlay/Android Auto
video keeps the phone's rendering. Factory-state screens and pairing/update
outcomes need their corresponding device state to validate visually; the asset
audit alone cannot confirm them.
