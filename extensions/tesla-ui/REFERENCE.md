# Instrument-cluster reference

The layout follows the Park and Driving illustrations in the
[Model S owner's manual](https://www.tesla.com/ownersmanual/models/en_us/GUID-29A7E205-A689-41D1-B69C-3AE821CB70E7.html).
The illustrated display area is 760 × 287 pixels, excluding numbered callouts.
Dashborne centers the main instrument positions on the 1920 × 480 display.
Cars and glyphs keep their proportions; the wider left area holds the power
history without overlapping the road.

Park has a large front three-quarter car, a clear background and a fixed bottom
status row. Driving uses a smaller elevated rear view, speed to its right and
a thin power meter below. The speed limit sits above the scene. The left map
area contains Dashborne's power history and available navigation scalars.

We reviewed Tesla's [Displays tutorial](https://www.tesla.com/support/videos/watch/displays-meet-your-model-s)
and [2025 Driver Controls video](https://player.vimeo.com/video/1094893003), linked
from [Meet Your Model S](https://www.tesla.com/support/meet-your-tesla/model-s).
The display tutorial's 13–42 second segment establishes the driving proportions.
For the Park transition, we extracted consecutive frames from 62–64 seconds of
the driver-controls video at its native 23.976 fps. The road clears before the
rear view turns into a larger three-quarter view; the footer stays in place.
The camera angle and editing limit precise timing measurements. Dashborne uses
a 1.067-second eased orbit, with 65 perspective renders and a reversible camera
parameter. This is a reconstruction, not an extracted Tesla animation.

Montserrat Regular supplies the live labels; Medium supplies the speed numerals. Glyph atlases
keep the device and host preview consistent without changing the official
app's font configuration. The font is an approximation of the reference's
geometric lettering; its spacing and outlines are not identical.

The Model 3 Highland mesh remains the user's chosen vehicle. Camera renders
preserve perspective and scale. Park/Drive transitions animate with closed
panels. Known open panels retain their posed endpoint during a view change.
Individual doors, frunk, trunk and the all-open pose have hinge animation;
other combinations retain accurate endpoint masks. Live panel readings still
need verification, so Preview is currently the way to exercise those poses.
