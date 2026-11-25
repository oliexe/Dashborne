# Theme engine validation

Target: stock OS **6.11.4**, checked on 25 November 2026 using resources pulled
from the connected DASH.

## Resource coverage

The capture contains 12 layouts and 1,419 PNGs, including Knob overlay sprites.
The audit classified 427 PNGs as authored replacements and 992 as palette
fallbacks, with no unsupported PNGs. Layout deltas passed their fingerprint and
round-trip checks against the capture.

The adapter routes ordinary images and both BitmapHelper overloads used by
text-embedded icons. Generated images preserve dimensions, transparency and
saturated colors. Host tests cover malformed images, filtering, override paths,
theme separation, native callback ownership and rollback.

## Device checks

The installed engine activated on 6.11.4 alongside Better Display Controls,
Development Menu and Screen Mirroring. Production mode supplied changing speed,
gear, navigation, temperature, media and warning data during the dark-mode pass.

Screenshots were checked for the dashboard, page editing, Stats/Actions/Special
picker categories, gauge and media configuration, and the main Settings pages.
Light-mode checks included the dashboard, embedded action icons, Display, Sound,
Controls, Indicators, Units, Bluetooth, System, Development and the update entry.
A temporary media widget was added, resized and removed through the stock UI.

This pass fixed overlapping speed and driving-mode text, gear-strip spacing,
stock Action-picker gradients, stale navigation-icon coloring, invisible
light-mode action icons, and low-contrast Restore defaults text.

## Final deployment check

After ADB reconnected, the final build, including the Knob fallback expansion,
was installed and activated on the device. Switching from light to dark mode
preserved readable Action-picker icons and media previews. Twelve page swipes
completed without a process restart or loss of ADB; available memory afterward
was about 96 MiB. This is a short navigation check, not a long-duration soak.

Dark mode was restored, Production mode remained disabled, and the temporary
media widget was removed from the dashboard.

## Remaining device checks

The keyboard, live Knob overlays and state-dependent pairing/update dialogs
still need visual checks on the device. Their resources passed the local audit
and layout tests. Resource coverage is not a claim that every possible UI state
has been visually verified.
