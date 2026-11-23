import subprocess
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
            result = subprocess.run(["sh", str(SCRIPT), str(root)], capture_output=True,
                                    text=True, timeout=10, check=True)
            for text in ["test-controller", "not-attached", "configfs", "peripheral",
                         "FILE /sys/class/zkswe_usb/zkswe0/enable\nabsent",
                         "Diagnostic snapshot complete"]:
                self.assertIn(text, result.stdout)
            after = {str(p.relative_to(root)): p.read_bytes() for p in root.rglob("*") if p.is_file()}
            self.assertEqual(after, fixture)


if __name__ == "__main__":
    unittest.main()
