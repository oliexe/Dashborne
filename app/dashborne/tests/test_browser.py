"""Real filesystem browsing, ELF checks, and one-shot launch recovery."""
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def app_elf():
    data = bytearray(512)
    data[:7] = b'\x7fELF\x01\x01\x01'
    struct.pack_into('<HHI', data, 16, 3, 40, 1)
    struct.pack_into('<I', data, 32, 64)
    struct.pack_into('<I', data, 36, 0x5000400)
    struct.pack_into('<HHH', data, 40, 52, 0, 0)
    struct.pack_into('<HH', data, 46, 40, 3)
    names = b'\0onEasyUIInit\0onStartupApp\0onEasyUIDeinit\0'
    struct.pack_into('<IIIIIIIIII', data, 104, 0, 11, 0, 0, 200, 48, 2, 0, 4, 16)
    struct.pack_into('<IIIIIIIIII', data, 144, 0, 3, 0, 0, 300, len(names), 0, 0, 1, 0)
    data[300:300+len(names)] = names
    for i, name in enumerate((b'onEasyUIInit', b'onStartupApp', b'onEasyUIDeinit')):
        struct.pack_into('<IIIBBH', data, 200+i*16, names.index(name), 400+i*4, 4, 0x12, 0, 1)
    return data


class BrowserTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory(prefix='dashborne-browser-')
        cls.work = Path(cls.tmp.name).resolve()
        cls.card = cls.work/'card with spaces'
        cls.request, cls.back = cls.work/'request', cls.work/'return'
        cls.binary = cls.work/'browser-test'
        subprocess.run([os.environ.get('HOST_CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror',
                        '-fsanitize=address,undefined', '-g',
                        f'-DBROWSER_ROOT="{cls.card}"', f'-DBROWSER_REQUEST="{cls.request}"',
                        f'-DBROWSER_RETURN="{cls.back}"', str(ROOT/'browser.c'),
                        str(ROOT/'tests/browser_test.c'), '-o', str(cls.binary)], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def setUp(self):
        shutil.rmtree(self.card, ignore_errors=True)
        (self.card/'Apps').mkdir(parents=True)
        (self.card/'Empty').mkdir()
        (self.card/'Apps/A sample.so').write_bytes(app_elf())
        (self.card/'Apps/readme.txt').write_text('Not an app')
        (self.card/'EasyUI.cfg').write_text('unchanged startup')
        (self.card/'outside').symlink_to(self.work)
        self.request.unlink(missing_ok=True); self.back.unlink(missing_ok=True)

    def run_case(self, name):
        subprocess.run([str(self.binary), name], check=True)
        self.assertEqual((self.card/'EasyUI.cfg').read_text(), 'unchanged startup')

    def valid(self, path):
        result = subprocess.run([str(self.binary), 'validate', str(path)])
        self.assertIn(result.returncode, (0, 1))
        return result.returncode == 0

    def test_browse_select_and_consumed_handoff(self):
        self.run_case('handoff')

    def test_deleted_or_unsupported_selection(self):
        self.run_case('unsupported'); self.run_case('removed')

    def test_validation_and_sd_boundary(self):
        path = self.card/'Apps/A sample.so'
        self.assertTrue(self.valid(path))
        outside = self.work/'outside.so'; outside.write_bytes(app_elf())
        (self.card/'Apps/escape.so').symlink_to(outside)
        self.assertFalse(self.valid(self.card/'Apps/escape.so'))
        own = self.card/'Dashborne/lib/libdashborne.so'; own.parent.mkdir(parents=True); own.write_bytes(app_elf())
        self.assertFalse(self.valid(own))
        for offset, value in [(4, 2), (5, 2), (16, 2), (18, 62), (37, 0), (212, 0x02), (213, 2)]:
            data = app_elf(); data[offset] = value; path.write_bytes(data)
            self.assertFalse(self.valid(path), (offset, value))
        for offset in (32, 120, 124, 128, 160, 164, 200):
            data = app_elf(); struct.pack_into('<I', data, offset, 0xffffffff); path.write_bytes(data)
            self.assertFalse(self.valid(path), offset)
        for length in (0, 20, 51, 100, 220, 310):
            path.write_bytes(app_elf()[:length]); self.assertFalse(self.valid(path), length)

    def test_bounded_large_folder(self):
        for i in range(300):
            (self.card/f'file-{i:03}').touch()
        self.run_case('limit')
