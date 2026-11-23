# Contrast

Adds **Contrast** at the bottom of the official dashboard's Settings categories.
Enable the plugin in Dashborne → Patches, launch the dashboard, then open Settings
and scroll its left-hand list to Contrast.

**Enable adjustments** applies the two sliders to the running display:

- **Contrast** changes the display engine's contrast, from 0 to 100.
- **Black level / luminance** shifts image luminance, from 0 to 100. It is not
  an independent gamma or calibrated black-point control, and leaves the
  backlight setting alone.

The grayscale strip helps compare shadows and highlights. Disabling adjustments
restores the values captured at plugin activation; **Reset** also returns the
slider positions to those values. Adjustments start disabled and are held only
for this process session.

## Implementation

The adapter supports the inspected official app 6.11.2 and FlyThings runtime.
It checks both complete library fingerprints and all ten callback addresses
before installing hooks. Other app/runtime builds are rejected.

An additive layout patch builds the panel from the installed `awlsettings.ftu`
on the microSD and mounts it through the extension engine. The menu adapters
append one row, delegate existing rows and controls to the official callbacks,
and connect the new sliders to the settings activity's existing listener ABI.

Display changes use the same volatile `/dev/disp` contrast and luminance ioctls
observed in the installed hardware helper. Each write is checked through the
corresponding getter. A partial failure restores both original values; a failed
restore remains retryable and blocks successful extension cleanup.

Normal cleanup restores the callbacks and captured display values. A full DASH
reboot resets the kernel display state, including when booting without the card.
Restarting only the UI process after a forced termination does not itself reset
kernel display values; use Reset before that operation or power-cycle the DASH.

Interface reference: [Allwinner display ioctl definitions](https://github.com/HakumenJean/AllwinnerT113S3/blob/master/drivers/hal/disp2/disp/sunxi_display2.h).

## Validation

```sh
sh extensions/contrast/test.sh
DASHBORNE_CC=/opt/homebrew/opt/llvm/bin/clang \
DASHBORNE_LD=/opt/homebrew/opt/lld/bin/ld.lld \
sh app/dashborne/build.sh
```

Tests cover the packaged panel, control bounds, delegated settings callbacks,
secondary listener offsets, selection changes, enable/reset behavior, display
readback, invalid values, partial-write rollback and cleanup retry. The C tests
run under ASan/UBSan. The ARM build and static analysis pass.

On the physical DASH, a native probe read contrast/luminance at 50/50, changed
contrast to 51, verified the driver readback and restored 50/50 successfully.
The user confirmed the hardware preview: the new Contrast settings page opens,
enabling adjustments and moving the Contrast slider changes the image, and Reset
returns it to normal. The separate luminance slider and reversal after booting
without the card have not yet been confirmed for this plugin.

To regenerate the additive panel against a local settings layout:

```sh
python3 extensions/contrast/generate.py /path/to/awlsettings.ftu
```
