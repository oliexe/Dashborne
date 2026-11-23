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
from author import scalar_patch
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


class PatchView(ctypes.Structure):
    _fields_ = [('data', ctypes.c_void_p), ('size', ctypes.c_uint)]
LIB.patch_layout_many.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.POINTER(PatchView), ctypes.c_uint,
                                 ctypes.POINTER(ctypes.c_void_p), ctypes.POINTER(ctypes.c_uint)]
def apply_many(base, patches):
    buffers = [ctypes.create_string_buffer(p) for p in patches]
    views = (PatchView*len(patches))(*(PatchView(ctypes.cast(b,ctypes.c_void_p),len(p)) for b,p in zip(buffers,patches)))
    pointer,size = ctypes.c_void_p(),ctypes.c_uint()
    ok=LIB.patch_layout_many(base,len(base),views,len(views),ctypes.byref(pointer),ctypes.byref(size))
    if not ok:
        assert not pointer.value and not size.value
        return None
    try: return ctypes.string_at(pointer,size.value)
    finally: LIB.free(pointer)


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


class ScalarTests(unittest.TestCase):
    def setUp(self):
        self.original={'id':0,'control':{'fontSize':28,'color':8618883,'label':'Fixture'},'values':[12,34]}
        self.base=encode(self.original)
        self.spec=scalar_patch(self.base,'font','Font','Test',[(('control','fontSize'),32)])
        self.packet=compile_patch(self.spec)

    def test_scalar_and_additive_edits_compose_without_other_changes(self):
        color=compile_patch(scalar_patch(self.base,'color','Color','Test',[(('control','color'),9352703)]))
        badge=json.loads((ROOT/'patches/badge.json').read_text());badge['base']=self.spec['base']
        expected=json.loads(json.dumps(self.original));expected['control'].update(fontSize=32,color=9352703)
        expected[badge['key']]=badge['control']
        self.assertEqual(decode(apply_many(self.base,[self.packet,color,compile_patch(badge)])),expected)
        self.assertEqual(decode(apply_many(self.base,[compile_patch(badge),color,self.packet])),expected)

    def test_conflicts_and_incorrect_expected_value_fail_entire_set(self):
        self.assertIsNone(apply_many(self.base,[self.packet,self.packet]))
        self.spec['edits'][0]['expect']=29
        self.assertIsNone(apply(self.base,compile_patch(self.spec)))
        self.assertIsNone(apply_many(self.base,[]))

    def test_corruption_truncation_bounds_and_oversized_edit(self):
        for n in range(len(self.packet)):self.assertIsNone(apply(self.base,self.packet[:n]))
        for at in range(len(self.packet)):
            bad=bytearray(self.packet);bad[at]^=128;self.assertIsNone(apply(self.base,bytes(bad)))
        for offset,value in [(28,0xffffffff),(32,0xffffffff),(32,0),(32,257)]:
            bad=bytearray(self.packet);struct.pack_into('<I',bad,offset,value)
            struct.pack_into('<I',bad,24,zlib.crc32(bad[28:]))
            self.assertIsNone(apply(self.base,bytes(bad)))

    def test_authoring_arrays_unicode_and_compiler_guards(self):
        data=encode({'text':'é','items':[12,34]})
        p=scalar_patch(data,'array','Array','Test',[(('items',1),56)])
        self.assertEqual(decode(apply(data,compile_patch(p))),{'text':'é','items':[12,56]})
        for value in [1000,{},float('nan')]:
            self.spec['edits'][0]['value']=value
            with self.assertRaises(ValueError):compile_patch(self.spec)
        self.spec['edits'][0]['value']=32;self.spec['edits']*=2
        with self.assertRaises(ValueError):compile_patch(self.spec)
        with self.assertRaises(ValueError):scalar_patch(self.base,'x','X','X',[(('control','fontSize'),100)])


if __name__ == '__main__':
    unittest.main()
