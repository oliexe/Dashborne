import json
import struct
import unittest
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class LayoutTest(unittest.TestCase):
    def test_loader_payload_and_touch_targets(self):
        blob = (ROOT / "card/ui/dashborne.ftu").read_bytes()
        self.assertEqual(blob[:6], b"ZKSW\xaa\x55")
        self.assertEqual(blob[-6:], b"\x55\xaaZKSW")
        offset, unpacked, packed = struct.unpack_from("<III", blob, 6)
        plain = zlib.decompress(blob[offset + 8:offset + 8 + packed - 16])
        self.assertEqual(len(plain), unpacked - 8)
        layout = json.loads(plain)
        self.assertEqual(layout, json.loads((ROOT / "screen.json").read_text()))
        controls = [v for k, v in layout.items() if "__" in k]
        ids = [v["id"] for v in controls]
        self.assertEqual(len(ids), len(set(ids)))
        self.assertTrue({50003, 20001, 20002}.issubset(ids))
        for control in controls:
            p = control["position"]
            self.assertTrue(0 <= p["left"] < p["left"] + p["width"] <= 1920)
            self.assertTrue(0 <= p["top"] < p["top"] + p["height"] <= 480)
        buttons = [v for k, v in layout.items() if k.startswith("button__")]
        self.assertEqual(len(buttons), 2)
        a, b = [v["position"] for v in buttons]
        self.assertLessEqual(a["top"] + a["height"], b["top"])


if __name__ == "__main__":
    unittest.main()
