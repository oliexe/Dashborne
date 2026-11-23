"""Build Dashborne's original layout for the FlyThings resource loader."""
import json
import struct
import zlib
from pathlib import Path

ROOT = Path(__file__).parent


def encode(layout):
    plain = json.dumps(layout, separators=(",", ":"), ensure_ascii=True).encode()
    compressed = zlib.compress(plain, 9)
    return (b"ZKSW\xaa\x55" + struct.pack("<III", 18, len(plain) + 8, len(compressed) + 16)
            + bytes(8) + compressed + b"\x55\xaaZKSW")


def decode(blob):
    if len(blob) < 32 or blob[:6] != b"ZKSW\xaa\x55" or blob[-6:] != b"\x55\xaaZKSW":
        raise ValueError("Invalid FTU envelope")
    offset, plain_size, packed_size = struct.unpack_from("<III", blob, 6)
    start, end = offset + 8, offset + 8 + packed_size - 16
    if start < 26 or end != len(blob) - 6 or plain_size < 8:
        raise ValueError("Invalid FTU lengths")
    plain = zlib.decompress(blob[start:end])
    if len(plain) != plain_size - 8:
        raise ValueError("Invalid uncompressed length")
    return json.loads(plain)


if __name__ == "__main__":
    layout = json.loads((ROOT / "screen.json").read_text())
    blob = encode(layout)
    assert decode(blob) == layout
    destination = ROOT / "card/ui/dashborne.ftu"
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(blob)
    print(f"Built {destination.name}: {len(blob)} bytes")
