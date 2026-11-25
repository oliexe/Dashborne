# Better Display Controls

Direct backlight control and separate day/night profiles for stock OS **6.11.4**.
Enable the extension in Dashborne, continue booting, then open **Extra → Better
Display Controls → Open**.

## Controls

- **Enable controls** previews your selected profile. If Auto has no valid clock,
  it selects Day so the preview takes effect. It starts off on a fresh install.
- **Mode** cycles between Auto, Day and Night. Auto follows the DASH's local clock;
  Day and Night apply a profile manually.
- Each profile has a **Brightness** slider, a **Black level** slider and a start
  time adjustable in 15-minute steps. Day runs until Night starts, including
  schedules that cross midnight.
- Moving a slider immediately previews that profile and selects manual Day or
  Night mode. **Keep changes** confirms and saves it within 15 seconds; otherwise
  the previous settings return. Back also cancels an unconfirmed preview.
- **Save changes** stores other edits in the card's root `dashborne.conf`. Back
  saves these edits too; a failed save keeps the page open for retry.
- **Restore stock** stops the override and restores stock display control.
  Save or Back remembers that choice.

Brightness exposes raw backlight levels **6–255**, bypassing the stock offset and
minimum. The slider floor is at least **2%** (6/255); older saved values below
that floor are raised when loaded. Level zero remains reserved for stock sleep.
The percentage is a fraction of the driver range, not a measured light output.

Black level uses the display engine's luminance control, **0–100**, with 50 neutral.
Lower values darken image levels and can hide shadow detail; this also affects
midtones and highlights. It is not a gamma curve or an adjustment to the LCD's
physical black floor. Lower the backlight first to reduce the glow of black pixels.

Auto waits for a valid DASH clock before applying either profile. Until time is
available, stock controls remain active; the page shows the clock status. The
extension uses the same system local time as the stock UI, including its time-zone
setting, and does not set the clock or require a separate internet connection.

## Configuration

All settings live in `dashborne.conf`:

| Key | Default | Values |
| --- | --- | --- |
| `display.enabled` | `0` | `0` stock controls, `1` override |
| `display.mode` | `0` | `0` Auto, `1` Day, `2` Night |
| `display.day_start` | `420` | Minutes after midnight, 07:00 |
| `display.night_start` | `1200` | Minutes after midnight, 20:00 |
| `display.day.brightness` | `160` | 6–255 |
| `display.night.brightness` | `16` | 6–255 |
| `display.day.black` | `50` | 0–100 |
| `display.night.black` | `50` | 0–100 |

The two start times must differ. Invalid settings fall back to disabled defaults.

## Implementation

The extension checks the stock app and hardware-library fingerprints before
activation. It temporarily changes the brightness helper's in-memory mapping,
so stock automatic-brightness updates and screen wake use the selected raw level.
Stock sleep still turns the backlight off. Restoring the mapping reapplies the
latest brightness requested by the stock controller.

Backlight and luminance changes use `/dev/disp` with readback verification.
The interface is described in the [Allwinner display header](https://github.com/HakumenJean/AllwinnerT113S3/blob/master/drivers/hal/disp2/disp/sunxi_display2.h).
Gamma-table readback returned an error on the tested DASH, so the extension does
not replace its gamma table. Display changes are volatile; settings are stored
only on the microSD card.

While this page is visible, it refreshes the stock display timeout. The native
LCD state governs hardware writes: a nonzero brightness readback does not mean
the panel is on. Preview rollback uses monotonic time, so clock synchronization
cannot extend an unconfirmed adjustment.

A native UI timer checks the clock once per second. Control changes run on the
UI thread, without a polling worker competing with the stock brightness writer.
Unloading removes its timers and hooks and restores the captured display state.

Run `sh extensions/better-display-controls/test.sh` for the local tests. Regenerate
the panel with `python3 extensions/better-display-controls/generate.py` after
editing its layout source.

Testing on 6.11.4 confirmed changes in the PWM hardware registers for brightness
and the output color-conversion registers for black level. Unconfirmed previews
returned to the original brightness and color coefficients after 15 seconds,
and the LCD remained enabled through repeated slider tests. The owner confirmed that
both sliders visibly change the physical screen after the timeout and preview fixes.
