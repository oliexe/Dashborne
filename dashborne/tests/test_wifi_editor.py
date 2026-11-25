import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class WifiEditorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory(prefix='dashborne-wifi-')
        cls.card = Path(cls.tmp.name)
        cls.binary = cls.card / 'test'
        subprocess.run([os.environ.get('HOST_CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror',
                        '-fsanitize=address,undefined', f'-DCONFIG_CARD="{cls.card}"',
                        str(ROOT / 'config.c'), str(ROOT / 'wifi_editor.c'),
                        str(ROOT / 'tests/wifi_editor_test.c'), '-o', str(cls.binary)], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def setUp(self):
        self.config = self.card / 'dashborne.conf'
        self.original = b'# Keep\nformat=1\nadb.enabled=0\ntweak.example.enabled=1\nfuture.setting=a=b\nwifi.ssid=Old\nwifi.password=oldpassword\n'
        self.config.write_bytes(self.original)

    def run_editor(self, *args, success=True):
        result = subprocess.run([str(self.binary), *args], capture_output=True)
        self.assertEqual(result.returncode, 0 if success else 1, result.stderr.decode())

    def test_keyboard_bounds_password_mask_and_cancel(self):
        self.run_editor('keyboard')
        self.assertEqual(self.config.read_bytes(), self.original)

    def test_atomic_save_preserves_other_settings_and_literal_password(self):
        self.run_editor('save', 'My Wi-Fi', ' "#=\\!$ word ')
        data = self.config.read_bytes()
        self.assertIn(b'wifi.ssid=My Wi-Fi\n', data)
        self.assertIn(b'wifi.password= "#=\\!$ word \n', data)
        self.assertIn(b'adb.enabled=0\ntweak.example.enabled=1\nfuture.setting=a=b\n', data)

    def test_invalid_and_failed_save_leave_config_untouched(self):
        for name, password in [('', 'password'), ('Network', 'short')]:
            self.run_editor('save', name, password, success=False)
            self.assertEqual(self.config.read_bytes(), self.original)
        pending = self.card / 'dashborne.conf.next'
        pending.mkdir()
        try:
            self.run_editor('save', 'New network', 'newpassword', success=False)
            self.assertEqual(self.config.read_bytes(), self.original)
        finally:
            pending.rmdir()
