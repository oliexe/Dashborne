import importlib.util
import json
from pathlib import Path
import struct
import unittest
from unittest.mock import patch
import subprocess
import tempfile
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
    def test_adb_reads_layout_in_memory_and_writes_only_patch(self):
        plain=b'{"id":0}'
        packed=zlib.compress(plain)
        base=b'ZKSW\xaa\x55'+struct.pack('<III',18,len(plain)+8,len(packed)+16)+bytes(8)+packed+b'\x55\xaaZKSW'
        with tempfile.TemporaryDirectory() as tmp:
            output=Path(tmp)/'panel.dbp'
            result=subprocess.CompletedProcess([],0,stdout=base,stderr=b'')
            with patch.object(gen.subprocess,'run',return_value=result) as run:
                gen.generate_from_device('test-device','test-adb',output)
            run.assert_called_once_with(['test-adb','-s','test-device','exec-out','cat','/res/ui/awlsettings.ftu'],
                                        check=True,capture_output=True,timeout=20)
            self.assertEqual(output.read_bytes(),gen.build(base))
            self.assertEqual(list(Path(tmp).iterdir()),[output])

    def test_adb_failure_or_invalid_layout_preserves_patch(self):
        with tempfile.TemporaryDirectory() as tmp:
            output=Path(tmp)/'panel.dbp';output.write_bytes(b'previous patch')
            for error in (subprocess.CalledProcessError(1,[]),subprocess.TimeoutExpired([],20)):
                with patch.object(gen.subprocess,'run',side_effect=error):
                    with self.assertRaises(subprocess.SubprocessError):
                        gen.generate_from_device('test-device','test-adb',output)
                self.assertEqual(output.read_bytes(),b'previous patch')
            with patch.object(gen.subprocess,'run',return_value=subprocess.CompletedProcess([],0,stdout=b'invalid',stderr=b'')):
                with self.assertRaises(struct.error):
                    gen.generate_from_device('test-device','test-adb',output)
            self.assertEqual(output.read_bytes(),b'previous patch')
if __name__=='__main__':unittest.main()
