# Controller input

## Stock behavior

Enhance's [6.11 release notes](https://www.enhauto.com/blogs/all/update-6-11-new-knob-dash-controls-features-automations),
published November 16, 2026, describe direct Knob control of the DASH:

| Knob input | Stock behavior |
| --- | --- |
| Rotate dial | Move between widgets; navigate within selected CarPlay/Android Auto |
| Press dial | Activate the selected widget or enter projection |
| Long-press side Back | Return from projection to the dashboard |
| Side arrow | Change dashboard page |

The [6.8 guide](https://www.enhauto.com/blogs/all/update-6-8-new-upgrades-for-tesla-model-3-y)
describes the right steering-wheel control: gentle scrolling moves selection,
a harder flick changes pages, right selects and left exits. It says this
navigation is disabled while Autopilot is active. The
[action-selection guide](https://enhauto.freshdesk.com/support/solutions/articles/202000116183-s3xy-dash-how-to-execute-actions-from-the-steering-wheel)
also confirms one-notch selection and rightward activation of an action widget.

These are documented behaviors, not a captured event mapping. We have not yet
verified the event codes, callback signatures, repeat behavior or dispatcher in
the installed 6.11.2 app. The input may arrive through the app's communication
layer rather than Linux input devices.

## Tesla UI handler

`cluster_window_input(action, navigation_allowed)` accepts logical previous,
next, select and Back actions on the UI thread. It returns whether it handled
the action. The fixed cluster currently has one interactive control, Back:
previous/next highlights it, select activates the focused control, and Back
returns directly to Extra. Unknown and page-change actions remain unhandled.

The handler ignores inactive and paused views, clears focus when navigation is
disallowed, and prevents repeated activation while closing. Pause clears focus.
The existing native Activity Back callback uses this path. Knob and scroll-wheel
events are **not connected yet**; tests supply logical actions directly.

The device adapter still needs to identify the stock UI navigation dispatcher,
preserve its enablement and Autopilot gates, normalize the verified events, and
route them to the foreground view. Handled events must not also activate a stock
widget behind Tesla UI. Release/repeat rules and callback thread ownership need
verification before attaching the adapter.

## ADB capture

With ADB enabled in Dashborne configuration, collect the input inventory:

```sh
python3 tools/capture_inputs.py inspect --output dashborne/build/input-captures/first
```

The default device is `10.0.1.35:5555`; use `--serial` for another ADB address.
The inventory records input devices, processes, local sockets and available
capture commands. It does not establish which input path the stock app uses.

If the device provides `timeout`, `nohup` and `cat`, start a trace:

```sh
python3 tools/capture_inputs.py start --seconds 90 --output dashborne/build/input-captures/first
```

It uses `getevent` when available, or reads each evdev node into a separate raw
file otherwise. The process writes to a unique directory on the card and stops
on the DASH after the requested duration. It can continue if launching the stock dashboard
disconnects ADB. A session file records its location before launch; check the
retrieved status and logs to confirm the collector actually ran.

With the car parked, launch the stock dashboard and leave a few seconds between
each input. Record the order: Knob clockwise/counterclockwise, dial press, side
Back hold, side arrow, then right-wheel up/down, right and left. Repeat with an
action widget selected and with projection selected. A touchscreen tap provides
a useful baseline if no controller events appear.

After the capture ends, return to Dashborne configuration, reconnect ADB and run:

```sh
python3 tools/capture_inputs.py pull --output dashborne/build/input-captures/first
```

Read `device/format.txt`, `runner.txt` and `exit.txt` together. The getevent path
produces `events.txt`; the fallback produces `eventN.bin`, `.error` and `.exit`
files for each input device. On the known ARMv7 platform, pull also decodes
complete raw records into `.jsonl` files. Other architectures and malformed
records remain raw for inspection. Check per-device errors as well as the
overall collector status.

An empty controller trace does not mean the Knob is unsupported. If only touch
events appear, the next device investigation must trace the stock navigation
callbacks. The tool neither injects input nor changes
the stock UI, network configuration or controller assignments.

The [getevent documentation](https://source.android.com/docs/core/interaction/input/getevent)
describes its timestamped kernel-event output. The
[Linux input documentation](https://docs.kernel.org/input/input.html#evdev)
describes the evdev interface. Availability of the capture commands on this DASH
still needs checking.
