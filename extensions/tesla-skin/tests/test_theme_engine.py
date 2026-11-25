import ctypes
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[1]


def chunk(kind, data):
    return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))


def png(kind, data, palette=b'', depth=8):
    parts = [b'\x89PNG\r\n\x1a\n', chunk(b'IHDR', struct.pack('>IIBBBBB', 2, 1, depth, kind, 0, 0, 0))]
    if palette:
        parts += [chunk(b'PLTE', palette), chunk(b'tRNS', b'\x00\xff')]
    parts += [chunk(b'IDAT', zlib.compress(data)), chunk(b'IEND', b'')]
    return b''.join(parts)


class EngineTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        output = Path(cls.temp.name) / 'theme.so'
        subprocess.run(['clang', '-shared', '-fPIC', '-Wall', '-Wextra', '-Werror',
                        str(ROOT / 'theme_engine.c'), '-lz', '-o', str(output)], check=True)
        cls.lib = ctypes.CDLL(str(output))
        cls.lib.theme_png.argtypes = [ctypes.c_char_p, ctypes.c_uint, ctypes.c_int, ctypes.c_uint,
                                     ctypes.c_uint, ctypes.POINTER(ctypes.c_void_p), ctypes.POINTER(ctypes.c_uint)]
        cls.free = ctypes.CDLL(None).free
        cls.free.argtypes = [ctypes.c_void_p]

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def convert(self, source, light=False):
        output, length = ctypes.c_void_p(), ctypes.c_uint()
        ok = self.lib.theme_png(source, len(source), light, 0x17181a, 0xf4f4f4, ctypes.byref(output), ctypes.byref(length))
        if not ok:
            self.assertFalse(output.value)
            self.assertEqual(length.value, 0)
            return None
        result = ctypes.string_at(output, length.value)
        self.free(output)
        return result

    def test_palette_alpha_and_warning_color(self):
        source = png(3, b'\0\0\1', b'\xff\xff\xff\xff\x00\x00')
        result = self.convert(source)
        self.assertIn(chunk(b'PLTE', b'\xf4\xf4\xf4\xff\x00\x00'), result)
        self.assertIn(chunk(b'tRNS', b'\x00\xff'), result)
        for index in (0, 20, 35, len(source)-1):
            corrupt = bytearray(source); corrupt[index] ^= 1
            self.assertIsNone(self.convert(bytes(corrupt)))
        for length in (0, 40, len(source)-1):
            self.assertIsNone(self.convert(source[:length]))

    def test_rgba_alpha_and_saturated_color(self):
        source = png(6, b'\0\xff\xff\xff\x80\xff\0\0\xff')
        result = self.convert(source)
        at = result.index(b'IDAT'); n = struct.unpack_from('>I', result, at-4)[0]
        self.assertEqual(zlib.decompress(result[at+4:at+4+n]), b'\0\xf4\xf4\xf4\x80\xff\0\0\xff')

    def test_rgba_sub_filter(self):
        source = png(6, b'\1\xff\xff\xff\xff\x01\x01\x01\x00')
        result = self.convert(source)
        at = result.index(b'IDAT'); n = struct.unpack_from('>I', result, at-4)[0]
        self.assertEqual(zlib.decompress(result[at+4:at+4+n]), b'\0\xf4\xf4\xf4\xff\x17\x18\x1a\xff')

    def test_theme_schema_rejects_behavior_changes(self):
        spec = importlib.util.spec_from_file_location('theme', ROOT / 'theme.py')
        module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
        theme = json.loads((ROOT / 'theme.json').read_text())
        theme['controls'] = {'dash1act': {'*': {'id': 123}}}
        path = Path(self.temp.name) / 'bad.json'; path.write_text(json.dumps(theme))
        with self.assertRaises(ValueError):
            module.load_theme(path)

    def test_arbitrary_override_paths_and_mode_cache(self):
        directory = Path(self.temp.name) / 'extension'
        relative = 'icons/new-user-icon.png'
        for mode in ('dash', 'light'):
            path = directory / 'assets/ui' / mode / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(png(6, b'\0\xff\xff\xff\xff\0\0\0\xff'))
        colors = ((ctypes.c_uint * 7) * 2)()
        self.lib.theme_resources_init.argtypes = [ctypes.c_char_p, ctypes.c_void_p]
        self.lib.theme_resource.argtypes = [ctypes.c_char_p, ctypes.c_int]
        self.lib.theme_resource.restype = ctypes.c_char_p
        self.lib.theme_resources_init(str(directory).encode(), colors)
        for mode in (0, 1, 0):
            result = self.lib.theme_resource(('/res/ui/dash/' + relative).encode(), mode)
            self.assertEqual(result.decode(), str(directory / 'assets/ui' / ('light' if mode else 'dash') / relative))
        knob = directory / 'assets/ui/light/Knob/icons_100/example.png'
        knob.parent.mkdir(parents=True, exist_ok=True)
        knob.write_bytes(png(6, b'\0\xff\xff\xff\xff\0\0\0\xff'))
        self.assertEqual(self.lib.theme_resource(b'/res/ui/Knob/icons_100/example.png', 1).decode(), str(knob))
        for unsafe in (b'dash/../outside.png', b'/unrelated/file.png', b'dash/icon.png.extra'):
            self.assertEqual(self.lib.theme_resource(unsafe, 0), unsafe)
        self.lib.theme_resources_clear()


if __name__ == '__main__':
    unittest.main()
