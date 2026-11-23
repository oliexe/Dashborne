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
        (self.card / "DASH_SD_PROBE_V1").write_bytes(b"DASH_SD_TEST\n")
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

    def test_missing_build_does_not_change_config(self):
        with self.assertRaises(OSError):
            stage(self.card, build=self.card / "missing")
        self.assertEqual((self.card / "EasyUI.cfg").read_bytes(), self.original)

    def test_wrong_card_does_not_change_config(self):
        (self.card / "DASH_SD_PROBE_V1").write_text("other")
        with self.assertRaises(ValueError):
            stage(self.card)
        self.assertEqual((self.card / "EasyUI.cfg").read_bytes(), self.original)

    def test_restore_does_not_overwrite_another_launcher(self):
        with self.assertRaises(ValueError):
            stage(self.card, restore=True)
        self.assertEqual((self.card / "EasyUI.cfg").read_bytes(), self.original)


if __name__ == "__main__":
    unittest.main()
