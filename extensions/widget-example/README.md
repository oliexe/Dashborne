# Widget example

A small widget registered in the stock dashboard picker. It displays a tap count
and its pixel dimensions, with independent state for each placement.

Enable **Widget Example** in Dashborne, continue booting, then open the dashboard
layout editor. Under **Special → Gauges**, choose a size. The stock editor places,
changes and removes it; the **Tap** button is hidden during editing.

## Create a widget

1. Copy this directory to `extensions/my-widget`.
2. Change the manifest's `id`, `title` and `description`, and the `WIDGET_ID` and
   `WIDGET_TITLE` definitions in `extension.c`. IDs must be unique.
3. Set `WIDGET_TAB`, `WIDGET_CATEGORY` and the size mask in the widget descriptor.
4. Replace the example's callbacks with your content and data handling.
5. Run `sh dashborne/build.sh`, then copy the built `Dashborne` directory to the
   card. The widget runtime is built into Dashborne.

The public interface is [dashborne_widget.h](../../dashborne/extensions/dashborne_widget.h).
An extension registers one widget definition. The runtime allocates a persistent
source ID in `dashborne.conf`, so loading extensions in a different order does not
change existing placements. Keep your widget ID unchanged across releases.

## Sizes

`DBW_ALL_SIZES` exposes every stock size. To offer a subset, combine bits such as
`(1u << DBW_1X1) | (1u << DBW_2X2)`. Each size appears as a picker choice, and choices
that cannot fit say “no space.”

| Size | Enum | Pixels |
| --- | --- | --- |
| 1 × 1 | `DBW_1X1` | 250 × 224 |
| 2 × 1 | `DBW_2X1` | 510 × 224 |
| 1 × 2 | `DBW_1X2` | 250 × 458 |
| 2 × 2 | `DBW_2X2` | 510 × 458 |
| 3 × 2 | `DBW_3X2` | 770 × 458 |
| 4 × 2 | `DBW_4X2` | 1030 × 458 |
| 5 × 2 | `DBW_5X2` | 1290 × 458 |
| 6 × 2 | `DBW_6X2` | 1550 × 458 |

The dashboard grid has six columns and two rows. Use `view->width` and
`view->height` when laying out content. The default canvas is inset 12 pixels on
each side, starts at y=54, and has height `view->height - 108`.

## Picker categories

Choose any row below. Category numbers are local to their tab, not global IDs.

| Tab | Categories, in index order starting at zero |
| --- | --- |
| `DBW_STATS` | Drive, Battery, Power, Temperature |
| `DBW_ACTIONS` | Open / Close, Drive, Charging, Mirrors, Heated Seats, Seat Cooling, Seat Adjustments, Climate Control, Lights, Traction Control, Lock/Unlock, Others |
| `DBW_SPECIAL` | Android Auto / CarPlay, Gauges, Navigation, Combined, Trip, TPMS, Media, Clock, Warnings |

A category changes where the widget is listed; it does not give the widget that
category's functionality. For example, a custom widget in the projection category
does not start CarPlay.

## Callbacks and controls

| Callback | Purpose |
| --- | --- |
| `create(view)` | Set up state in `view->user`. Return zero on success. |
| `update(view, microseconds)` | Refresh data and controls. Check visibility and throttle expensive work. |
| `draw(view, canvas)` | Optional custom drawing within the content canvas. |
| `click(view)` | Handle the action button. Omit it to hide the button. |
| `destroy(view)` | Release state, including after a failed `create`. |
| `suspend(view)` | Release caches or pause workers when the dashboard pauses; `update` resumes afterward. |

Callbacks run on the UI thread. Views are pooled across three moving page
containers, so `destroy`/`create` can occur when a page is reused. Do not treat a
view index as a persistent placement ID or retain its control pointers afterward.

The runtime supplies a window, canvas, title, value, status and action button.
Use `widgets->text` to update text and `widgets->invalidate` to request a canvas
repaint. For other native control operations, resolve them through the extension
host. Keep UI calls out of background workers and release workers before unload.

A widget definition and its callbacks must remain valid until it is unregistered.
Unloading while the dashboard is live is refused. Extensions receive no display
wake lock or network changes from the widget runtime.

## Layout storage

All custom widgets share one card layout in `dashborne.conf`. The existing
`vehicle-widget.layout.*` keys are retained for compatibility with earlier cards;
`widgets.source.*` keys store the registry's stable IDs.

A disabled widget remains an unavailable placeholder while another widget
extension is active. Its placement can be removed normally. With no widget
extensions enabled, or when booting without Dashborne, the dashboard uses the
flash layout.

See the [runtime verification notes](../../dashborne/core/widget-runtime/README.md) for test coverage
and remaining hardware checks.
