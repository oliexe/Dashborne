import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from stage import STARTUP, stage


class StageTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.card = Path(self.temp.name)
        self.original = b'{"resPath":"/res/ui/","startupLibPath":"previous.so","keepMe":42}\n'
        (self.card / "EasyUI.cfg").write_bytes(self.original)

    def test_install_repeat_and_exact_restore(self):
        stage(self.card)
        stage(self.card)
        config = json.loads((self.card / "EasyUI.cfg").read_bytes())
        self.assertEqual(config["startupLibPath"], STARTUP)
        self.assertEqual(config["keepMe"], 42)
        self.assertEqual((self.card / "Dashborne/EasyUI.previous.cfg").read_bytes(), self.original)
        self.assertTrue((self.card / "lib/libdashborne.so").is_file())
        self.assertTrue((self.card / "ui/dashborne.ftu").is_file())
        self.assertTrue((self.card / "Dashborne/usb-diagnostics.sh").is_file())
        stage(self.card, restore=True)
        self.assertEqual((self.card / "EasyUI.cfg").read_bytes(), self.original)

    def test_wifi_config_and_preservation(self):
        local = self.card / "local.json"
        local.write_text(json.dumps({"ssid": "Example WiFi", "password": "example-passphrase"}))
        stage(self.card, wifi_config=local)
        expected = b"Example WiFi\nexample-passphrase\n"
        self.assertEqual((self.card / "Dashborne/wifi.conf").read_bytes(), expected)
        stage(self.card)
        self.assertEqual((self.card / "Dashborne/wifi.conf").read_bytes(), expected)

    def test_invalid_wifi_does_not_change_launcher(self):
        local = self.card / "local.json"
        for values in [{"ssid": "x", "password": "short"},
                       {"ssid": "bad\nnetwork", "password": "example-passphrase"},
                       {"ssid": "x" * 33, "password": "example-passphrase"},
                       {"ssid": 42, "password": "example-passphrase"}]:
            local.write_text(json.dumps(values))
            with self.assertRaises(ValueError):
                stage(self.card, wifi_config=local)
            self.assertEqual((self.card / "EasyUI.cfg").read_bytes(), self.original)
            self.assertFalse((self.card / "lib/libdashborne.so").exists())

    def test_missing_build_does_not_change_config(self):
        with self.assertRaises(OSError):
            stage(self.card, build=self.card / "missing")
        self.assertEqual((self.card / "EasyUI.cfg").read_bytes(), self.original)

    def test_wrong_resource_root_does_not_change_config(self):
        other = b'{"resPath":"/other/ui/","startupLibPath":"other.so"}\n'
        (self.card / "EasyUI.cfg").write_bytes(other)
        with self.assertRaises(ValueError):
            stage(self.card)
        self.assertEqual((self.card / "EasyUI.cfg").read_bytes(), other)

    def test_restore_does_not_overwrite_another_launcher(self):
        with self.assertRaises(ValueError):
            stage(self.card, restore=True)
        self.assertEqual((self.card / "EasyUI.cfg").read_bytes(), self.original)


if __name__ == "__main__":
    unittest.main()
