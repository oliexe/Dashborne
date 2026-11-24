"""Read-only inventory against missing, populated and damaged system sources."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

class DeviceTests(unittest.TestCase):
    def test_inventory(self):
        with tempfile.TemporaryDirectory(prefix='dashborne-device-') as tmp:
            root = Path(tmp)
            binary = root/'device-test'
            subprocess.run([os.environ.get('HOST_CC','cc'), '-std=c11', '-Wall', '-Wextra', '-Werror',
                            '-fsanitize=address,undefined', '-g', f'-DDEVICE_ROOT="{root}"',
                            str(ROOT/'tests/device_test.c'), '-lz', '-o', str(binary)], check=True)
            def put(name, data):
                path = root/name
                path.parent.mkdir(parents=True,exist_ok=True)
                path.write_text(data)
            def run(case):
                before = {p:p.read_bytes() for p in root.rglob('*') if p.is_file()}
                subprocess.run([str(binary),case],check=True)
                self.assertEqual(before,{p:p.read_bytes() for p in root.rglob('*') if p.is_file()})
            run('empty')
            put('etc/build.prop','ro.system.version=test-2.3\n')
            put('data/zkapp.cfg','{"proddate":"202601011230","dev_name":"Test DASH"}')
            put('proc/meminfo','MemTotal: 262144 kB\nMemAvailable: 131584 kB\n')
            put('proc/uptime','176520.12 456789.00\n')
            put('sys/block/mmcblk0/device/type','SD\n')  # SD is not always mmcblk1.
            put('sys/block/mmcblk0/device/serial','0x11223344\n')
            put('sys/block/mmcblk0/size','1073741824\n')
            put('res/lib/libzkgui.so','Unrecognized app fixture')
            run('populated')
            put('data/zkapp.cfg','x'*9000)
            put('proc/meminfo','MemTotal: bad\n')
            put('proc/uptime','184467440737095516160\n')
            run('malformed')
