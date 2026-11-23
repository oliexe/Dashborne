"""Exercise the native patch engine with original synthetic layouts."""
import ctypes
import json
from pathlib import Path
import struct
import sys
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT.parent / 'dashboard'))
from package import compile_patch
from layout import encode, decode

LIB = ctypes.CDLL(str(ROOT / 'build/patch-test.so'))
LIB.patch_layout.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_void_p,
                            ctypes.c_uint, ctypes.POINTER(ctypes.c_void_p), ctypes.POINTER(ctypes.c_uint)]
LIB.free.argtypes = [ctypes.c_void_p]


def apply(base, patch):
    pointer, size = ctypes.c_void_p(), ctypes.c_uint()
    result = LIB.patch_layout(base, len(base), patch, len(patch), ctypes.byref(pointer), ctypes.byref(size))
    if not result:
        assert not pointer.value and not size.value
        return None
    try:
        return ctypes.string_at(pointer, size.value)
    finally:
        LIB.free(pointer)


class PatchTests(unittest.TestCase):
    def setUp(self):
        self.original = {'id': 0, 'textview__1': {'id': 50001, 'text': 'Synthetic fixture'}}
        self.base = encode(self.original)
        plain = zlib.decompress(self.base[26:-6])
        self.spec = json.loads((ROOT / 'patches/badge.json').read_text())
        self.spec['base'].update(size=len(self.base), crc32=f'{zlib.crc32(self.base):08x}',
                                 plain_size=len(plain), plain_crc32=f'{zlib.crc32(plain):08x}')
        self.patch = compile_patch(self.spec)

    def test_only_appends_our_control(self):
        result = decode(apply(self.base, self.patch))
        added = result.pop(self.spec['key'])
        self.assertEqual(added, self.spec['control'])
        self.assertEqual(result, self.original)

    def test_patch_contains_our_fragment_not_base(self):
        self.assertNotIn(b'Synthetic fixture', self.patch)
        self.assertIn(b'DASHBORNE', self.patch)

    def test_different_version_and_already_patched_are_rejected(self):
        self.assertIsNone(apply(encode({'id': 1}), self.patch))
        once = apply(self.base, self.patch)
        self.assertIsNone(apply(once, self.patch))

    def test_truncated_and_corrupted_inputs_are_rejected(self):
        for n in range(len(self.patch)):
            self.assertIsNone(apply(self.base, self.patch[:n]))
        for n in range(len(self.base)):
            self.assertIsNone(apply(self.base[:n], self.patch))
        for at in range(len(self.patch)):
            changed = bytearray(self.patch); changed[at] ^= 0x80
            self.assertIsNone(apply(self.base, bytes(changed)))

    def test_envelope_extensions_are_preserved(self):
        base = bytearray(self.base[:26] + bytes(2) + self.base[26:])
        struct.pack_into('<I', base, 6, 20)
        self.spec['base'].update(size=len(base), crc32=f'{zlib.crc32(base):08x}')
        result = apply(bytes(base), compile_patch(self.spec))
        self.assertEqual(result[18:28], base[18:28])
        self.assertIn(self.spec['key'], decode(result))

    def test_untrusted_lengths_and_invalid_envelopes(self):
        for offset in (6, 10, 14):
            base = bytearray(self.base)
            struct.pack_into('<I', base, offset, 0xffffffff)
            self.spec['base']['crc32'] = f'{zlib.crc32(base):08x}'
            self.assertIsNone(apply(bytes(base), compile_patch(self.spec)))
        for offset in (12, 20):
            patch = bytearray(self.patch)
            struct.pack_into('<I', patch, offset, 0xffffffff)
            self.assertIsNone(apply(self.base, bytes(patch)))


if __name__ == '__main__':
    unittest.main()
