# Tesla Skin

Tesla-inspired typography, flat surfaces and controls for the stock DASH interface.
It keeps the dashboard layout and native widget behavior, with light and dark
palettes that follow **Settings → Configuration → Display theme**.

Enable **Tesla Skin** in Dashborne's boot menu, then continue booting. Theme
changes take effect when you leave stock Settings; reopen Settings to see its
matching palette. Disable the extension and restart to return to the stock style.

## Appearance

- Neutral dashboard cards, larger digital speed, simpler gauges and media controls.
- Matching Settings, widget picker, page editor, dialogs and keyboard.
- Inter for ordinary interface text. Native fonts remain in data labels that use
  special unit glyphs.
- Existing warnings, vehicle indicators and action symbols retain their meaning.

The skin adapts the Model 3/Y visual style to the DASH's wide display, using
Inter and original artwork. The dashboard layout remains the stock DASH layout.

## Implementation

The adapter supports stock app **6.11.2** and its verified FlyThings runtime.
Binary and layout fingerprints reject other builds. It compiles small layout
deltas against the installed resources at startup, then redirects layout reads
and theme pointers in the UI process. Shared picker and navigation artwork uses
a theme-aware image loader in the same process. Activity names, control IDs, callbacks and
vehicle-data sources stay native.

`generate.py` draws the PNG resources from the palette and geometry definitions.
`prepare.py` builds the deltas from a fresh ADB capture of the seven named `.ftu`
layouts. Run these from the repository root:

```sh
python3 extensions/tesla-skin/generate.py  # requires Pillow
python3 extensions/tesla-skin/prepare.py /path/to/adb-layout-capture
sh extensions/tesla-skin/test.sh
```

To also check the generated deltas against your captured layouts:

```sh
TESLA_SKIN_CAPTURE=/path/to/adb-layout-capture sh extensions/tesla-skin/test.sh
```

## Design references

The palette, spacing and control treatment draw on the supplied Model 3 UI
component sheets by Namig Oeku, the [Model 3 touchscreen manual](https://www.tesla.com/ownersmanual/model3/en_us/GUID-518C51C1-E9AC-4A68-AE12-07F4FF8C881E.html),
and [recent Model 3 screen footage](https://www.youtube.com/watch?v=hhyQ7-8KMmE).
The bundled [Inter font](https://github.com/rsms/inter) uses the
[SIL Open Font License](assets/LICENSE.txt).
