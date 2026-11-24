import importlib.util
import json
from pathlib import Path
import shlex
import subprocess
import struct
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('capture', Path(__file__).parents[1] / 'capture_inputs.py')
capture = importlib.util.module_from_spec(spec)
spec.loader.exec_module(capture)


class CaptureTests(unittest.TestCase):
    def test_bounded_detached_capture(self):
        remote = '/mnt/extsd/Dashborne/input-capture-0123456789ab'
        script = capture.start_script(remote, 90)
        subprocess.run(['sh', '-n'], input=script, text=True, check=True)
        self.assertIn('nohup sh -c ', script)
        runner = shlex.split(script[script.index('nohup sh -c '):])[3]
        subprocess.run(['sh', '-n'], input=runner, text=True, check=True)
        self.assertIn('timeout 90 getevent -lt', runner)
        self.assertIn('timeout 90 cat "$device"', runner)
        self.assertIn('exit.txt', runner)
        self.assertIn('finished.txt', runner)
        for seconds in (0, -1, 181):
            with self.assertRaises(ValueError): capture.start_script(remote, seconds)
        with self.assertRaises(ValueError): capture.start_script(remote + '; reboot', 90)

    def test_inventory_keeps_failures_and_uses_argument_lists(self):
        def run(argv, **kwargs):
            self.assertEqual(argv[:3], ['adb', '-s', 'host:5555'])
            self.assertEqual(kwargs['timeout'], 20)
            self.assertNotIn('shell', kwargs)
            if argv[-1] == 'get-state': return subprocess.CompletedProcess(argv, 0, 'device\n', '')
            return subprocess.CompletedProcess(argv, 1, '', 'not available')
        with tempfile.TemporaryDirectory() as tmp, patch.object(capture.subprocess, 'run', side_effect=run):
            capture.inspect('host:5555', Path(tmp))
            result = json.loads((Path(tmp) / 'inventory.json').read_text())
            self.assertEqual(len(result['commands']), len(capture.INSPECTIONS))
            self.assertEqual(result['commands']['devices']['exit'], 1)

    def test_lost_transport_preserves_session_and_rejects_reuse(self):
        with tempfile.TemporaryDirectory() as tmp, patch.object(capture, 'inspect'), \
                patch.object(capture, 'adb', side_effect=RuntimeError('offline')):
            output = Path(tmp)
            with self.assertRaisesRegex(RuntimeError, 'offline'):
                capture.start('host:5555', output, 60)
            session = json.loads((output / 'session.json').read_text())
            self.assertEqual(session['state'], 'requested')
            capture.remote_path(session['remote'])
            with self.assertRaisesRegex(RuntimeError, 'already contains'):
                capture.start('host:5555', output, 60)

    def test_raw_event_decode_and_truncated_records(self):
        data = struct.pack('<llHHi', 42, 1234, 2, 8, -1)
        self.assertEqual(capture.decode_arm_evdev(data), [dict(seconds=42, microseconds=1234, type=2, code=8, value=-1)])
        self.assertEqual(capture.decode_arm_evdev(b''), [])
        with self.assertRaises(ValueError): capture.decode_arm_evdev(data[:-1])
        with self.assertRaises(ValueError): capture.decode_arm_evdev(struct.pack('<llHHi', 42, -1, 2, 8, 0))


if __name__ == '__main__':
    unittest.main()
