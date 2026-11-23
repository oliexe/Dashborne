import subprocess
import os
import tempfile
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parents[1] / "usb-diagnostics.sh"


class UsbDiagnosticsTest(unittest.TestCase):
    def test_configfs_controller_without_legacy_control(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            fixture = {
                "proc/filesystems": b"nodev\tconfigfs\nnodev\tfunctionfs\n",
                "sys/class/udc/test-controller/state": b"not-attached\n",
                "sys/class/udc/test-controller/maximum_speed": b"high-speed\n",
                "sys/bus/platform/devices/usb-test/of_node/dr_mode": b"peripheral\0",
                "sys/kernel/config/usb_gadget/g1/UDC": b"\n",
            }
            for name, data in fixture.items():
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(data)
            # Reproduce the device's limited utility set: ls is the only external command.
            commands = root / "commands"
            commands.mkdir()
            (commands / "ls").symlink_to("/bin/ls")
            result = subprocess.run(["/bin/sh", str(SCRIPT), str(root)], capture_output=True,
                                    env={**os.environ, "PATH": str(commands)},
                                    text=True, timeout=10, check=True)
            for text in ["test-controller", "not-attached", "configfs", "peripheral",
                         "FILE /sys/class/zkswe_usb/zkswe0/enable\nabsent",
                         "Diagnostic snapshot complete"]:
                self.assertIn(text, result.stdout)
            self.assertNotIn("not found", result.stderr)
            after = {str(p.relative_to(root)): p.read_bytes() for p in root.rglob("*") if p.is_file() and not p.is_symlink()}
            self.assertEqual(after, fixture)


if __name__ == "__main__":
    unittest.main()
