import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('import_resources',ROOT/'import_resources.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)

class ImportTests(unittest.TestCase):
    def test_supported_and_unmapped_controls(self):
        layout={'id':0,'resolution':{'width':1920,'height':480},
                'textview__speed':{'id':50020,'text':'130','touchable':True},
                'textview__warning':{'id':50017,'text':'sample','visible':True},
                'textview__gear':{'id':50040,'backgroundPic':'home/gear.png','text':''}}
        original=repr(layout);assets=set();out=m.adapt(layout,0,assets)
        self.assertEqual(repr(layout),original)
        self.assertEqual(out['textview__speed']['text'],'--')
        self.assertFalse(out['textview__speed']['touchable'])
        self.assertNotIn('textview__warning',out)
        self.assertNotIn('backgroundPic',out['textview__gear'])
        self.assertEqual(out['textview__gear']['id'],599913)

    def test_path_escape_rejected(self):
        for path in ('../outside.png','/tmp/outside.png','..\\outside.png'):
            with self.assertRaises(ValueError):m.adapt({'backgroundPic':path},0,set())

    def test_import_and_missing_asset(self):
        with tempfile.TemporaryDirectory() as t:
            root=Path(t);source=root/'source';(source/'ui').mkdir(parents=True)
            layout={'id':0,'resolution':{'width':1920,'height':480},
                    'textview__1':{'id':50020,'text':'99','backgroundPic':'digit.png'}}
            for name in ('dashsimple.ftu','dash5.ftu'):(source/'ui'/name).write_bytes(m.encode(layout))
            with self.assertRaises(ValueError):m.prepare(source,root/'missing')
            self.assertFalse((root/'missing').exists())
            (source/'ui/digit.png').write_bytes(b'test asset')
            out=m.prepare(source,root/'output')
            self.assertEqual(m.decode((root/'output/viewer.ftu').read_bytes()),out)
            self.assertNotEqual(out['window__style0']['textview__1']['id'],out['window__style1']['textview__1']['id'])
            with self.assertRaises(ValueError):m.prepare(source,root/'output')

    def test_repo_output_rejected(self):
        with self.assertRaises(ValueError):m.prepare('/tmp',ROOT/'assets')

if __name__=='__main__':unittest.main()
