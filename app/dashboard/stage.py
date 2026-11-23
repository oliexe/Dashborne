"""Install a dashboard build on the existing test card, or restore its config."""
import argparse
import json
import os
import struct
from pathlib import Path

from layout import decode

ROOT = Path(__file__).resolve().parent
STARTUP = "/mnt/extsd/lib/libdashborne.so"


def atomic_write(path, content):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".tmp")
    with temporary.open("wb") as stream:
        stream.write(content)
        stream.flush()
        os.fsync(stream.fileno())
    temporary.replace(path)


def stage(card, restore=False, build=None):
    card = Path(card).resolve()
    if not card.is_dir() or (card / "DASH_SD_PROBE_V1").read_bytes() != b"DASH_SD_TEST\n":
        raise ValueError("Select the existing DASH test card")
    config_path = card / "EasyUI.cfg"
    previous = card / "Dashborne/EasyUI.previous.cfg"
    raw_config = config_path.read_bytes()
    config = json.loads(raw_config)
    if restore:
        if config.get("startupLibPath") != STARTUP:
            raise ValueError("The active configuration does not select Dashborne")
        backup = previous.read_bytes()
        json.loads(backup)
        atomic_write(config_path, backup)
        return
    if config.get("resPath") != "/res/ui/":
        raise ValueError("This build expects the installed resource root /res/ui/")
    build = Path(build) if build else ROOT / "card"
    library = (build / "lib/libdashborne.so").read_bytes()
    if (len(library) < 52 or library[:6] != b"\x7fELF\x01\x01" or
            struct.unpack_from("<H", library, 18)[0] != 40 or
            not struct.unpack_from("<I", library, 36)[0] & 0x400):
        raise ValueError("Expected a 32-bit ARM hard-float library; run build.sh")
    layout = (build / "ui/dashborne.ftu").read_bytes()
    decode(layout)
    if config.get("startupLibPath") != STARTUP:
        atomic_write(previous, raw_config)
    elif not previous.is_file():
        raise ValueError("The previous launcher configuration is missing")
    atomic_write(card / "lib/libdashborne.so", library)
    atomic_write(card / "ui/dashborne.ftu", layout)
    config["startupLibPath"] = STARTUP
    atomic_write(config_path, (json.dumps(config, indent=2) + "\n").encode())


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("card", type=Path)
    parser.add_argument("--restore", action="store_true")
    args = parser.parse_args()
    try:
        stage(args.card, args.restore)
    except (OSError, ValueError) as error:
        parser.exit(1, f"{error}\n")
    print("Previous launcher restored." if args.restore else "Dashborne staged. Eject the card before disconnecting it.")
