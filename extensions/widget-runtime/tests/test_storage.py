import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

class LayoutStorageTests(unittest.TestCase):
    def test_round_trip_and_failures(self):
        with tempfile.TemporaryDirectory(prefix='dashborne-layout-') as temp:
            root = Path(temp)
            stock, ram = root/'stock.json', root/'runtime.json'
            config = root/'dashborne.conf'
            config.write_text('format=1\nadb.enabled=0\nfuture.key=keep me\n')
            stock.write_text('{"original": true}\n')
            runner = root/'main.c'
            runner.write_text('#include "storage.h"\nint main(int n,char **v){(void)n;return v[1][0]==\'p\'?!!layout_prepare():!!layout_save();}\n')
            binary = root/'test'
            subprocess.run(['clang','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                f'-DCONFIG_CARD="{root}"',f'-DLAYOUT_STOCK="{stock}"',f'-DLAYOUT_RAM="{ram}"',
                '-I',str(ROOT),str(runner),str(ROOT/'storage.c'),str(ROOT/'settings.c'),'-o',str(binary)],check=True)
            def run(action, success=True):
                result = subprocess.run([str(binary),action],capture_output=True,env={**os.environ,'UBSAN_OPTIONS':'halt_on_error=1'})
                self.assertEqual(result.returncode,0 if success else 1,result.stderr.decode())
            run('prepare');self.assertEqual(ram.read_bytes(),stock.read_bytes())
            original_stock = stock.read_bytes()
            for content in [b'{"car":[529],"escaped":"a\\nb"}\n', b'x'*12000, b'{"small":1}']:
                ram.write_bytes(content);run('save')
                self.assertIn(b'future.key=keep me',config.read_bytes())
                self.assertIn(b'adb.enabled=0',config.read_bytes())
                ram.unlink();run('prepare');self.assertEqual(ram.read_bytes(),content)
                self.assertEqual(stock.read_bytes(),original_stock)
            saved = config.read_bytes()
            ram.write_bytes(b'x'*12001);run('save',False);self.assertEqual(config.read_bytes(),saved)
            ram.write_bytes(b'{"changed":1}');(root/'dashborne.conf.next').mkdir()
            run('save',False);self.assertEqual(config.read_bytes(),saved)
            (root/'dashborne.conf.next').rmdir()
            config.write_text('format=1\nvehicle-widget.layout.bytes=4\nvehicle-widget.layout.0=00zz\n')
            before = ram.read_bytes();run('prepare',False);self.assertEqual(ram.read_bytes(),before)
            config.write_text('format=1\nvehicle-widget.layout.bytes=9999999999999999999\n')
            run('prepare',False)
            config.write_text('format=1\nvehicle-widget.layout.bytes=4\n')
            run('prepare',False)
            config.write_text('format=1\n#'+('x'*32700))
            ram.write_bytes(b'{"space":1}');run('save',False)
            self.assertEqual(stock.read_bytes(),original_stock)

if __name__ == '__main__':
    unittest.main()
