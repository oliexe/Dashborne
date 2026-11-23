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
        self.assertTrue({50003, 50004, 20001, 20002, 20003}.issubset(ids))
        for control in controls:
            p = control["position"]
            self.assertTrue(0 <= p["left"] < p["left"] + p["width"] <= 1920)
            self.assertTrue(0 <= p["top"] < p["top"] + p["height"] <= 480)
        buttons = [v for k, v in layout.items() if k.startswith("button__")]
        self.assertEqual(len(buttons), 3)
        for i, first in enumerate(buttons):
            a = first["position"]
            for second in buttons[i + 1:]:
                b = second["position"]
                self.assertTrue(a["left"] + a["width"] <= b["left"] or
                                b["left"] + b["width"] <= a["left"] or
                                a["top"] + a["height"] <= b["top"] or
                                b["top"] + b["height"] <= a["top"])


if __name__ == "__main__":
    unittest.main()
