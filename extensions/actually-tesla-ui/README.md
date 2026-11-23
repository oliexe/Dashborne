# Actually Tesla UI

A native Dashborne theme for the installed dashboard. It applies Inter typography,
quieter widget surfaces, large digital values, compact labels, thin indicators,
blue selection states and coordinated light/dark graphics. Live values, widget
selection, page navigation and actions continue to come from the official app.

## Use

In Dashborne → Patches, disable **Dashborne badge**, enable **Actually Tesla UI**,
and launch the dashboard. Both extensions currently replace the dashboard layout,
so they cannot be selected together. The theme follows the official appearance
selection, including its background-free variant.

Disable the plugin and restart to return to the stock theme. Booting without
Dashborne also restores the original appearance. The theme does not change the
saved widget arrangement or the official app's connectivity settings.

## Widget coverage

| Family | Treatment |
| --- | --- |
| Values and actions, 1×1 / 1×2 / 2×1 | Compact labels, clearer value hierarchy, neutral cards and blue selection borders. |
| Vertical and horizontal indicators | Thin tracks, blue values and green regeneration graphics. |
| Combined values | Three aligned readings separated by fine rules. |
| Speed gauges A/B and digital gauge D | Large digital speed, reduced decorative graphics, native gear/subvalue controls. |
| Power gauge C | Thin power arcs and a restrained pointer. |
| Media | Inter track/artist text and newly drawn transport controls. |
| Navigation | Compact text and thin progress graphics; native destination and ETA bindings. |
| Tire pressure, 1×1 / 2×2 | Original top-view vehicle diagram, four native pressure readings and compact units. |
| Trips | Neutral summary surface and separation between title/actions and readings. |
| Analog/digital clocks | New clock face and hands, more balanced digital time typography. |
| Phone projection, every width | Neutral frame and a simple phone placeholder; projected content remains controlled by the phone. |

The layout changes cover the top, main and bottom page templates. Existing
semantic action icons, warning/telltale artwork and phone-projected content remain
provided by the running app. The settings application is a separate screen; this
plugin currently themes dashboard widgets.

## Reference

The [Tesla Dashboard UI Component Library](https://www.figma.com/design/esiPKaosjQeF53gdDltcks/Tesla-Dashboard-UI-Component-Library--Community-?node-id=69-19501)
by Namig Oeku was inspected in Figma: typography, color guidance, icons, buttons,
tabs/fields, music bars, trip directions, parked/driving sidebars, menu screens,
main elements and the complete display/menu mockups. Its layouts are adapted to
the DASH's 1920×480 display and configurable widget grid.

The inspected heading style specifies Inter Semi Bold, 46px / 56px, with 1%
tracking. Other sizes include 36, 28, 24, 22, 18, 16 and 14px. The implementation
uses an Inter Regular face with the runtime's bold styling, and enlarges driving
values for this display. Colors use the reference's neutral family and blue
`#2E78FF`; active regeneration uses `#25CB55`.

The bundled [Inter 4.1](https://github.com/rsms/inter/releases/tag/v4.1) font is
unmodified and distributed under its [SIL Open Font License](assets/LICENSE.txt).
Widget chrome and diagrams are drawn by `generate.py`.

## Implementation

- `assets/layout.atp` contains 1,335 guarded edits. At activation the plugin reads
  the installed layout, verifies the file and decompressed-content fingerprints,
  applies the edits, and mounts the generated layout through Dashborne's recovery
  journal. The layout contains the original control identities and bindings.
- A version-checked adapter temporarily redirects resource pointers and colors
  in all four official theme tables: light/dark, with/without backgrounds. This
  lets normal widget refresh and appearance changes retain the theme.
- The runtime registers the bundled font and temporarily routes default and
  numeric font creation to it. Named multilingual fallback fonts are retained.
  Cleanup restores the font routing and theme tables. The extra registered font
  remains available until the UI process exits.
- Current native profile: installed app library, 3,464,120 bytes, CRC32
  `9de99542`; runtime, 826,052 bytes, CRC32 `ece0feec`. Different binaries fail
  activation before any private-address access. The inspected 6.11.4 layout is
  identical to the test unit's 6.11.2 layout; that does not imply binary adapter
  compatibility with the 6.11.4 app library.

## Development and verification

Normal Dashborne builds package the checked-in theme assets and layout delta.
To regenerate graphics, install Pillow and run `python3 generate.py`. To author
the layout delta against a local reference, run:

```sh
python3 extensions/actually-tesla-ui/prepare.py /path/to/dash1act.ftu
sh extensions/actually-tesla-ui/test.sh
DASHBORNE_LAYOUT_REFERENCE=/path/to/dash1act.ftu \
  python3 extensions/actually-tesla-ui/tests/test_theme.py
```

Tests cover transformation round trips, preserved control identities/state/input
flags, new properties, every input truncation, wrong fingerprints, CRC failures,
bounds errors, asset dimensions and 10,000 mutations under ASan/UBSan. The optional
local-reference check verifies the actual delta against the inspected layout.
The ARM build and static analysis pass. Device preview validation is in progress.
