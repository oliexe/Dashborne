import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from install import install, STARTUP, BACKUP, WIFI


class InstallTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.card = Path(self.tmp.name)
        (self.card / 'DASH_SD_PROBE_V1').write_bytes(b'DASH_SD_TEST\n')
        self.original = b'{"resPath":"/res/ui/","startupLibPath":"/mnt/extsd/lib/libdashborne.so","keep":42}\n'
        (self.card / 'EasyUI.cfg').write_bytes(self.original)
        self.config = self.card / 'input.json'
        self.config.write_text(json.dumps({'ssid':'Example WiFi','password':'example-passphrase'}))

    def test_install_repeat_restore(self):
        changed = install(self.card, self.config)
        self.assertEqual(changed[-1], 'EasyUI.cfg')
        current = json.loads((self.card / 'EasyUI.cfg').read_bytes())
        self.assertEqual(current['startupLibPath'], STARTUP)
        self.assertEqual(current['keep'], 42)
        self.assertEqual((self.card / WIFI).read_bytes(), b'Example WiFi\nexample-passphrase\n')
        install(self.card)
        self.assertEqual((self.card / BACKUP).read_bytes(), self.original)
        self.assertEqual(install(self.card, restore=True), ['EasyUI.cfg'])
        self.assertEqual((self.card / 'EasyUI.cfg').read_bytes(), self.original)

    def test_invalid_input_preserves_launcher(self):
        for data in ({'ssid':'network','password':'short'}, {'ssid':'a\nb','password':'example-passphrase'}):
            self.config.write_text(json.dumps(data))
            with self.assertRaises(ValueError):
                install(self.card, self.config)
            self.assertEqual((self.card / 'EasyUI.cfg').read_bytes(), self.original)
            self.assertFalse((self.card / BACKUP).exists())

    def test_missing_build_preserves_launcher(self):
        with self.assertRaises(OSError):
            install(self.card, self.config, build=self.card / 'absent')
        self.assertEqual((self.card / 'EasyUI.cfg').read_bytes(), self.original)

    def test_no_restore_over_other_launcher(self):
        with self.assertRaises(ValueError):
            install(self.card, restore=True)

    def test_centered_layout_has_no_controls(self):
        layout = json.loads((ROOT / 'screen.json').read_bytes())
        widgets = [value for key,value in layout.items() if '__' in key]
        self.assertEqual(len(widgets),3)
        self.assertFalse(any(key.startswith('button__') for key in layout))
        for widget in widgets:
            self.assertFalse(widget['touchable'])
            box = widget['position']
            self.assertEqual(box['left'] + box['width'] / 2, 960)
            self.assertEqual(widget['alignment'],37)
            self.assertLessEqual(box['top'] + box['height'],480)

if __name__ == '__main__':
    unittest.main()
