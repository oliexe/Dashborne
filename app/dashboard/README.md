# First Dashborne screen

A 1920 × 480 screen with a touch counter and an **Open original dashboard**
button. It runs inside the installed FlyThings process and forwards the original
initialization and shutdown callbacks. Vehicle data is not connected yet.

## Build and check

Requires Python 3, LLVM Clang and LLD with ARM support. On macOS with Homebrew:

```sh
DASHBORNE_CC=/opt/homebrew/opt/llvm/bin/clang \
DASHBORNE_LD=/opt/homebrew/opt/lld/bin/ld.lld \
sh app/dashboard/build.sh
sh app/dashboard/test.sh
```

The output is `card/lib/libdashborne.so` and `card/ui/dashborne.ftu` under this
directory. `screen.json` is the editable layout source. The build creates its
link dependency stub locally; the running app loads `/res/lib/libzkgui.so`.

The local tests use a simulated framework with address and undefined-behavior
sanitizers. They cover lifecycle forwarding, touch counting, original-screen
navigation, allocation/layout failures and compatibility fallbacks. They also
check the compressed layout and touch-target bounds. They do not verify the
display driver, physical touch input or interactions with the live services.

## Runtime integration

The adapter registers `dashborneActivity` with the installed activity factory.
It retains the framework's activity implementation and overrides layout selection,
creation and click handling. It uses a small ABI adapter instead of a separate
graphics stack.

This first implementation targets the exact runtime captured from the test DASH:
826,052 bytes, FNV-1a-32 `3ca7071f`. That fingerprint is a compatibility check.
Missing bindings, a different runtime, or an unreadable layout select the original
startup screen. Layout creation is attempted before selecting Dashborne, allowing
a reported creation failure to fall back too. A framework crash cannot be caught
by this fallback.

Resource names are relative to the framework's resource root. The layout name
resolves from either `/res/ui/` or `/mnt/extsd/ui/` to our file on the card.
Original resource paths stay available. The screen's prepared instance is handed
to the factory; its second `BaseApp::create` call returns the already-created
window. The inspected runtime supports this path, but the complete lifecycle
still needs verification on the DASH.

Events append to `/mnt/extsd/Dashborne/events.log`. After testing, check for
`screen-created`, `dashborne-selected`, `touch`, and `open-original`.

## Physical test

1. Prepare the existing test card using the staging command described below.
2. With the DASH powered off, insert the card and power it on.
3. Check the screen orientation, tap the test button several times, and verify
   that the counter follows each tap.
4. Open the original dashboard and check its normal behavior.
5. Power off before removing the card. Starting without it selects the installed
   dashboard. Reconnect the card to the laptop to inspect the event log.

Rendering, physical touch, original-dashboard navigation and shutdown remain
pending hardware tests.
