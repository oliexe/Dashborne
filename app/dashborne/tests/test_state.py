import ctypes
import importlib.util
import json
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[1]

def load_module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
    return module

codec = load_module('fixture_codec', ROOT.parent / 'dashboard/layout.py')
patcher = load_module('fixture_patcher', ROOT.parent / 'injector/package.py')

class Entry(ctypes.Structure):
    _fields_ = [('id', ctypes.c_char*32), ('title', ctypes.c_char*64), ('description', ctypes.c_char*128),
                ('enabled', ctypes.c_int), ('compatible', ctypes.c_int), ('native', ctypes.c_int)]
class Settings(ctypes.Structure):
    _fields_ = [('wifi_adb', ctypes.c_int), ('count', ctypes.c_uint), ('patches', Entry*64)]


class StateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.root = Path(cls.temp.name)
        stub = cls.root/'mount.c'
        stub.write_text('int core_available; int extension_probe(const char*id){return id[0]==110 || (core_available && id[0]==101);}\nint mount(const char*a,const char*b,const char*c,unsigned long d,const void*e){(void)a;(void)b;(void)c;(void)d;(void)e;return 0;}\nint umount2(const char*a,int b){(void)a;(void)b;return 0;}\n')
        output = cls.root/'state.so'
        subprocess.run(['cc','-shared','-fPIC','-std=c11', '-Wall','-Wextra','-Werror',
                        '-DDASHBORNE_ROOT="'+str(cls.root/'card')+'"',
                        '-DDASHBORNE_TARGET="'+str(cls.root/'base.ftu')+'"',
                        str(ROOT/'state.c'),str(ROOT.parent/'injector/patch.c'),str(stub),'-lz','-o',str(output)],check=True)
        cls.lib = ctypes.CDLL(str(output))
        cls.lib.state_load.argtypes = [ctypes.POINTER(Settings)]
        cls.lib.state_save.argtypes = [ctypes.POINTER(Settings)]
        cls.logger = ctypes.CFUNCTYPE(None,ctypes.c_char_p)(lambda s: None)
        cls.lib.state_apply.argtypes = [ctypes.POINTER(Settings),type(cls.logger)]

    @classmethod
    def tearDownClass(cls): cls.temp.cleanup()

    def setUp(self):
        ctypes.c_int.in_dll(self.lib,"core_available").value=0
        self.card = self.root/'card'
        if self.card.exists(): shutil.rmtree(self.card)
        (self.card/'patches').mkdir(parents=True)
        self.original={'id':0,'textview__1':{'id':50001,'text':'Synthetic base'}}
        self.base=codec.encode(self.original); (self.root/'base.ftu').write_bytes(self.base)
        plain=zlib.decompress(self.base[26:-6])
        p=json.loads((ROOT.parent/'injector/patches/badge.json').read_text())
        p['base'].update(size=len(self.base),crc32=f'{zlib.crc32(self.base):08x}',plain_size=len(plain),plain_crc32=f'{zlib.crc32(plain):08x}')
        (self.card/'patches/one.dbp').write_bytes(patcher.compile_patch(p))
        p['key']='textview__900002';p['control']['id']=59002;p['control']['text']='SECOND'
        (self.card/'patches/two.dbp').write_bytes(patcher.compile_patch(p))
        (self.card/'patches/catalog.tsv').write_text('one\tFirst patch\tA label\ntwo\tSecond patch\tAnother label\n')

    def load(self):
        s=Settings();self.lib.state_load(ctypes.byref(s));return s

    def test_settings_roundtrip_and_composed_patches(self):
        s=self.load(); self.assertEqual(s.count,2)
        self.assertTrue(all(s.patches[i].compatible for i in range(2)))
        self.assertFalse(s.wifi_adb or s.patches[0].enabled)
        s.wifi_adb=1;s.patches[0].enabled=1;s.patches[1].enabled=1
        self.assertEqual(self.lib.state_save(ctypes.byref(s)),1)
        s=self.load();self.assertEqual(s.wifi_adb,1);self.assertEqual(s.patches[1].enabled,1)
        self.assertEqual(self.lib.state_apply(ctypes.byref(s),self.logger),2)
        result=codec.decode((self.card/'generated.ftu').read_bytes())
        self.assertEqual(result.pop('textview__900001')['text'],'DASHBORNE')
        self.assertEqual(result.pop('textview__900002')['text'],'SECOND')
        self.assertEqual(result,self.original)
        self.assertEqual((self.root/'base.ftu').read_bytes(),self.base)

    def test_missing_incompatible_and_removed_patches(self):
        (self.card/'settings.conf').write_text('wifi_adb=1\npatch.one=1\npatch.two=1\n')
        (self.card/'patches/two.dbp').unlink()
        s=self.load();self.assertEqual(s.patches[1].compatible,0);self.assertEqual(s.patches[1].enabled,0)
        (self.card/'patches/one.dbp').write_bytes(b'broken')
        self.assertEqual(self.lib.state_apply(ctypes.byref(s),self.logger),-1)
        self.assertFalse((self.card/'generated.ftu').exists())

    def test_native_entries_are_separate_from_layout_patches(self):
        (self.card/'patches/catalog.tsv').write_text('native\tNative module\tExample\tnative\nwrong\tBad kind\tSkip\tother\n')
        (self.card/'settings.conf').write_text('patch.native=1\n')
        s=self.load();self.assertEqual(s.count,1)
        self.assertTrue(s.patches[0].native and s.patches[0].compatible and s.patches[0].enabled)
        self.assertEqual(self.lib.state_apply(ctypes.byref(s),self.logger),0)
        self.assertFalse((self.card/'generated.ftu').exists())

    def test_scalar_and_additive_settings_batch_and_collision(self):
        author=load_module('patch_author',ROOT.parent/'injector/author.py')
        spec=author.scalar_patch(self.base,'scalar','Scalar','Example',[(('textview__1','id'),50002)])
        (self.card/'patches/scalar.dbp').write_bytes(patcher.compile_patch(spec))
        (self.card/'patches/catalog.tsv').write_text('one\tBadge\tExample\nscalar\tScalar\tExample\n')
        s=self.load()
        for i in range(s.count):s.patches[i].enabled=1
        self.assertEqual(self.lib.state_apply(ctypes.byref(s),self.logger),2)
        changed=codec.decode((self.card/'generated.ftu').read_bytes())
        self.assertEqual(changed['textview__1']['id'],50002)
        self.assertEqual(changed['textview__900001']['text'],'DASHBORNE')
        (self.card/'patches/one.dbp').write_bytes(patcher.compile_patch(spec))
        (self.card/'generated.ftu').unlink()
        self.assertEqual(self.lib.state_apply(ctypes.byref(s),self.logger),-1)
        self.assertFalse((self.card/'generated.ftu').exists())

    def test_core_button_is_composed_without_a_user_toggle(self):
        ctypes.c_int.in_dll(self.lib,"core_available").value=1
        core=self.card/'extensions/extra-menu/assets';core.mkdir(parents=True)
        (core/'dashboard.dbp').write_bytes((self.card/'patches/one.dbp').read_bytes())
        s=self.load();self.assertEqual(self.lib.state_apply(ctypes.byref(s),self.logger),1)
        self.assertIn('textview__900001',codec.decode((self.card/'generated.ftu').read_bytes()))
        s.patches[1].enabled=1
        self.assertEqual(self.lib.state_apply(ctypes.byref(s),self.logger),2)
        result=codec.decode((self.card/'generated.ftu').read_bytes())
        self.assertIn('textview__900001',result);self.assertIn('textview__900002',result)
        (core/'dashboard.dbp').unlink()
        self.assertEqual(self.lib.state_apply(ctypes.byref(s),self.logger),-1)

    def test_unselected_does_not_generate_overlay(self):
        s=self.load();self.assertEqual(self.lib.state_apply(ctypes.byref(s),self.logger),0)
        self.assertFalse((self.card/'generated.ftu').exists())

    def test_malformed_catalog_and_settings(self):
        (self.card/'patches/catalog.tsv').write_text('../escape\tBad\tBad\none\tFirst\tOK\none\tDuplicate\tIgnored\ninvalid line\n')
        (self.card/'settings.conf').write_text('\np\np.\npatch.\npatch.one=1\nwifi_adb=1\nwifi_adb=0\n')
        s=self.load();self.assertEqual(s.count,1);self.assertFalse(s.wifi_adb);self.assertTrue(s.patches[0].enabled)
        self.assertEqual(s.patches[0].id,b'one')

    def test_full_catalog_settings_roundtrip(self):
        ids = ['patch-'+str(i).zfill(25) for i in range(64)]
        packet = (self.card/'patches/one.dbp').read_bytes()
        for id in ids: (self.card/'patches'/f'{id}.dbp').write_bytes(packet)
        (self.card/'patches/catalog.tsv').write_text(''.join(
            id+'\t'+'Title'*12+'\t'+'Description '*10+'\n' for id in ids))
        s = self.load(); self.assertEqual(s.count, 64)
        for i in range(64): s.patches[i].enabled = i % 2
        self.assertEqual(self.lib.state_save(ctypes.byref(s)), 1)
        restored = self.load()
        self.assertEqual([restored.patches[i].enabled for i in range(64)], [i % 2 for i in range(64)])
