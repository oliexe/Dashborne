"""Encode and decode FlyThings layout resources."""
import json
import struct
import zlib


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
    if start < 25 or end != len(blob) - 6 or plain_size < 8:
        raise ValueError("Invalid FTU lengths")
    plain = zlib.decompress(blob[start:end])
    if len(plain) != plain_size - 8:
        raise ValueError("Invalid uncompressed length")
    return json.loads(plain)
