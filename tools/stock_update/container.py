#!/usr/bin/env python3
"""Verify and extract the single-partition S3XY DASH ZKSWE update container.

Static file processing only; does not mount, execute, repack or flash firmware.
Format reference: atomicstack/tc002-customisation, tc002-update-img.py.
The DASH payload is an ext-family filesystem, not that device's SquashFS.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zlib


def decode(data):
    if len(data) < 588 or data[:9] != b"ZKSWEV1.0":
        raise ValueError("Not a supported ZKSWE container")
    if data[0x10:0x13] != bytes((0x30, 1, 0x30)):
        raise ValueError("Only the observed single-entry layout is supported")
    if struct.unpack_from("<I", data, 0x30)[0] != 0x20C:
        raise ValueError("Unexpected information-block size")
    stored_crc = struct.unpack_from("<I", data, 0x238)[0]
    if zlib.crc32(data[:0x238]) != stored_crc:
        raise ValueError("Header CRC32 mismatch")
    offset, length = struct.unpack_from("<II", data, 0x18)
    if offset != 0x23C or length < 16 or offset + length != len(data):
        raise ValueError("Unexpected payload bounds or trailing data")
    payload = data[0x20:0x30] + data[offset + 16:offset + length]
    stored_md5 = data[offset:offset + 16].hex()
    if hashlib.md5(payload).hexdigest() != stored_md5:
        raise ValueError("Payload MD5 mismatch")
    code = struct.unpack_from("<I", data, 0x35)[0]
    metadata = {
        "container_sha256": hashlib.sha256(data).hexdigest(),
        "partition_index": data[0x14],
        "device_code": hex(code),
        "device_flag": hex(data[0x39]),
        "payload_offset": offset,
        "payload_size": length,
        "header_crc32": hex(stored_crc),
        "payload_md5": stored_md5,
        "payload_sha256": hashlib.sha256(payload).hexdigest(),
        "ext_superblock_magic_valid": payload[1080:1082] == b"\x53\xef",
    }
    return payload, metadata


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path)
    parser.add_argument("output", type=Path, nargs="?", help="New output file; omit for verification only")
    args = parser.parse_args()
    payload, metadata = decode(args.image.read_bytes())
    if args.output:
        with args.output.open("xb") as output:
            output.write(payload)
    print(json.dumps(metadata, indent=2))


if __name__ == "__main__":
    main()
