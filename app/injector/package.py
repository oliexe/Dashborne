"""Compile separate layout edits; no base layout is needed to build them."""
import json
import re
from pathlib import Path
import struct
import zlib

ROOT = Path(__file__).resolve().parent
TARGET = "/res/ui/dash1act.ftu"
MAX_FILE = 2 * 1024 * 1024


def scalar(value):
    if type(value) not in (str, int, float, bool, type(None)):
        raise ValueError('Expected a JSON scalar')
    raw = json.dumps(value, ensure_ascii=True, allow_nan=False, separators=(',', ':')).encode()
    if len(raw) > 256:
        raise ValueError('Scalar too long')
    return raw


def compile_patch(spec, *, native_controls=False):
    if spec["target"] != TARGET or spec["operation"] not in ("append-root-control", "replace-scalars"):
        raise ValueError("Unsupported patch operation or target")
    base = spec["base"]
    if not (32 <= base["size"] <= MAX_FILE and 2 <= base["plain_size"] <= MAX_FILE):
        raise ValueError("Invalid base lengths")
    if spec["operation"] == "append-root-control":
        key = spec["key"]
        if native_controls:
            if not re.fullmatch(r'(button|window|textview)__[a-zA-Z0-9_]+', key):
                raise ValueError('Invalid native control key')
        else:
            if not key.startswith("textview__") or not key.removeprefix("textview__").isdigit():
                raise ValueError("Expected a textview key")
            if spec["control"].get("touchable") is not False:
                raise ValueError("The additive patch must be noninteractive")
        fragment = ("," + json.dumps(key) + ":" + json.dumps(
            spec["control"], separators=(",", ":"), ensure_ascii=True)).encode()
        magic = b"DBP1"
    else:
        edits = spec['edits']
        if not 1 <= len(edits) <= 1024:
            raise ValueError('Expected 1..1024 scalar edits')
        fragment = bytearray(); end = 0
        for edit in sorted(edits, key=lambda e: e['offset']):
            at = edit['offset']
            before, after = scalar(edit['expect']), scalar(edit['value'])
            if len(before) != len(after) or before == after:
                raise ValueError('Scalar edits require different values with equal encoded lengths')
            if type(at) is not int or at < end or at + len(before) > base['plain_size']:
                raise ValueError('Overlapping or out-of-bounds edit')
            fragment += struct.pack('<II', at, len(before)) + before + after
            end = at + len(before)
        magic = b"DBP2"
    if len(fragment) > 16384:
        raise ValueError("Patch fragment too large")
    fields = (base["size"], int(base["crc32"], 16), base["plain_size"],
              int(base["plain_crc32"], 16), len(fragment), zlib.crc32(fragment))
    return magic + struct.pack("<6I", *fields) + fragment


if __name__ == "__main__":
    out = ROOT / "build/badge.dbp"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(compile_patch(json.loads((ROOT / "patches/badge.json").read_text())))
    print(f"Built {out.name}: {out.stat().st_size} bytes")
