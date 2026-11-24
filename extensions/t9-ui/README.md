# T9 UI

An experimental port of the T9 minimal and circular dashboard layouts into
Dashborne's Extra menu. Tap **Style** to switch layouts and **Back** to return.

The current adapter reads speed, battery percentage, range and gear from the
installed DASH app. It uses the app's unit conversions and clears readings when
the Commander disconnects. The circular layout shows battery level around its
centre dial; its speed needle, trip panels, warning lights and other unmapped
controls are omitted. Fonts come from the DASH's active font configuration.

Both layouts have rendered on the DASH and switched during a device preview.
The data adapter reads the stock app's cached values. Live updates still need
testing with a connected vehicle.

## Prepare the layouts

From the repository root, pass an extracted T9 resource directory containing
`ui/dashsimple.ftu` and `ui/dash5.ftu` to the importer:

```sh
python3 extensions/t9-ui/import_resources.py /path/to/t9-resources /path/to/t9-import
```

Copy the resulting directory to `Dashborne/imports/t9-ui` on the microSD card.
Build and install Dashborne normally, select **T9 UI (experimental)** in Patches,
then open **Extra → T9 UI → Open**.

The importer adapts layouts and copies their referenced images. The extension
uses native DASH controls and its existing data service; it does not start the
T9 application. Compatibility is checked against the tested DASH app and UI
runtime before activation.
