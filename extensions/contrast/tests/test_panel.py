import importlib.util
import json
from pathlib import Path
import struct
import unittest
import zlib
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('contrast_generate',ROOT/'generate.py')
gen=importlib.util.module_from_spec(spec);spec.loader.exec_module(gen)
class PanelTests(unittest.TestCase):
    def test_shipped_patch(self):
        b=(ROOT/'assets/panel.dbp').read_bytes()
        self.assertEqual(b[:4],b'DBP1')
        fields=struct.unpack_from('<6I',b,4)
        self.assertEqual(fields[4],len(b)-28)
        self.assertEqual(fields[5],zlib.crc32(b[28:]))
        content=json.loads(b'{'+b[29:]+b'}')
        self.assertEqual(content,{'window__dashborne_contrast':gen.panel()})
    def test_controls_and_touch_bounds(self):
        p=gen.panel();self.assertFalse(p['visible'])
        for v in p.values():
            if not isinstance(v,dict) or 'position' not in v:continue
            r=v['position'];self.assertGreaterEqual(r['left'],0);self.assertGreaterEqual(r['top'],0)
            self.assertLessEqual(r['left']+r['width'],1000);self.assertLessEqual(r['top']+r['height'],480)
        self.assertEqual(p['seekbar__contrast']['max'],100)
        self.assertEqual(p['seekbar__luminance']['max'],100)
if __name__=='__main__':unittest.main()
