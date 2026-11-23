"""Encode the helper's original screen layout."""
import json
import sys
from pathlib import Path
ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT.parent / "dashboard"))
from layout import encode, decode
if __name__ == "__main__":
    layout = json.loads((ROOT / "screen.json").read_text())
    data = encode(layout)
    assert decode(data) == layout
    destination = ROOT / "card/ui/adb-enabler.ftu"
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(data)
    print(f"Built {destination.name}: {len(data)} bytes")
