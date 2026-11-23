"""Compile an additive layout patch; no base layout is needed to build it."""
import json
from pathlib import Path
import struct
import zlib

ROOT = Path(__file__).resolve().parent
TARGET = "/res/ui/dash1act.ftu"
MAX_FILE = 2 * 1024 * 1024


def compile_patch(spec):
    if spec["target"] != TARGET or spec["operation"] != "append-root-control":
        raise ValueError("Unsupported patch operation or target")
    base = spec["base"]
    if not (32 <= base["size"] <= MAX_FILE and 2 <= base["plain_size"] <= MAX_FILE):
        raise ValueError("Invalid base lengths")
    key = spec["key"]
    if not key.startswith("textview__") or not key.removeprefix("textview__").isdigit():
        raise ValueError("Expected a textview key")
    if spec["control"].get("touchable") is not False:
        raise ValueError("The initial additive patch must be noninteractive")
    fragment = ("," + json.dumps(key) + ":" + json.dumps(
        spec["control"], separators=(",", ":"), ensure_ascii=True)).encode()
    if len(fragment) > 16384:
        raise ValueError("Patch fragment too large")
    fields = (base["size"], int(base["crc32"], 16), base["plain_size"],
              int(base["plain_crc32"], 16), len(fragment), zlib.crc32(fragment))
    return b"DBP1" + struct.pack("<6I", *fields) + fragment


if __name__ == "__main__":
    out = ROOT / "build/badge.dbp"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(compile_patch(json.loads((ROOT / "patches/badge.json").read_text())))
    print(f"Built {out.name}: {out.stat().st_size} bytes")
