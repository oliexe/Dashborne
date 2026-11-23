"""Install ADB Enabler on a mounted SD card or a DASH connected over ADB."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import struct
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT.parent / "dashboard"))
from stage import atomic_write, wifi_config_bytes
from layout import decode

STARTUP = "/mnt/extsd/lib/libadb-enabler.so"
BACKUP = "adb-enabler/EasyUI.previous.cfg"
WIFI = "adb-enabler/wifi.conf"
PAYLOAD = ("lib/libadb-enabler.so", "ui/adb-enabler.ftu")


def install(card, wifi_config=None, restore=False, build=None):
    card = Path(card)
    if (card / "DASH_SD_PROBE_V1").read_bytes() != b"DASH_SD_TEST\n":
        raise ValueError("Select the existing DASH test card")
    raw = (card / "EasyUI.cfg").read_bytes()
    config = json.loads(raw)
    if restore:
        if config.get("startupLibPath") != STARTUP:
            raise ValueError("ADB Enabler is not the selected launcher")
        backup = (card / BACKUP).read_bytes()
        json.loads(backup)
        atomic_write(card / "EasyUI.cfg", backup)
        return ["EasyUI.cfg"]
    if config.get("resPath") != "/res/ui/":
        raise ValueError("Expected the installed resource root /res/ui/")
    wifi = wifi_config_bytes(wifi_config) if wifi_config else (card / WIFI).read_bytes()
    # Validate the card format as well as JSON input before changing anything.
    parts = wifi.split(b"\n")
    if (len(parts) != 3 or parts[2] or not 1 <= len(parts[0]) <= 32 or
            not 8 <= len(parts[1]) <= 63 or b"\0" in wifi or b"\r" in wifi):
        raise ValueError("Wi-Fi config needs an SSID line and a WPA passphrase line")
    build = Path(build) if build else ROOT / "card"
    library, layout = [(build / name).read_bytes() for name in PAYLOAD]
    if (len(library) < 52 or library[:6] != b"\x7fELF\x01\x01" or
            struct.unpack_from("<H", library, 18)[0] != 40 or
            not struct.unpack_from("<I", library, 36)[0] & 0x400):
        raise ValueError("Expected an ARM hard-float library; run build.sh")
    decode(layout)
    if config.get("startupLibPath") != STARTUP:
        atomic_write(card / BACKUP, raw)
    elif not (card / BACKUP).is_file():
        raise ValueError("Previous launcher configuration is missing")
    atomic_write(card / WIFI, wifi)
    for name, data in zip(PAYLOAD, (library, layout)):
        atomic_write(card / name, data)
    config["startupLibPath"] = STARTUP
    atomic_write(card / "EasyUI.cfg", (json.dumps(config, indent=2) + "\n").encode())
    # The launcher is switched only after all dependencies have been uploaded.
    return [BACKUP, WIFI, *PAYLOAD, "EasyUI.cfg"]


def deploy(device, adb, wifi_config=None, restore=False):
    command = [adb, "-s", device]
    def run(*args):
        return subprocess.run(command + list(args), check=True, capture_output=True)
    with tempfile.TemporaryDirectory() as tmp:
        card = Path(tmp) / "card"
        card.mkdir()
        for name in ("DASH_SD_PROBE_V1", "EasyUI.cfg", BACKUP, WIFI):
            target = card / name
            target.parent.mkdir(parents=True, exist_ok=True)
            result = subprocess.run(command + ["pull", "/mnt/extsd/" + name, str(target)], capture_output=True)
            if result.returncode and name in ("DASH_SD_PROBE_V1", "EasyUI.cfg"):
                raise ValueError("Cannot read the DASH SD card configuration")
        changed = install(card, wifi_config, restore)
        for name in changed:
            remote = "/mnt/extsd/" + name
            pending = remote + ".next"
            run("shell", "mkdir -p " + shlex.quote(str(Path(remote).parent)))
            run("push", str(card / name), pending)
            verification = Path(tmp) / "verification"
            run("pull", pending, str(verification))
            if verification.read_bytes() != (card / name).read_bytes():
                raise ValueError("Upload verification failed")
            run("shell", "mv " + shlex.quote(pending) + " " + shlex.quote(remote))
        run("shell", "sync && setprop ctl.restart zkswe")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    destination = parser.add_mutually_exclusive_group(required=True)
    destination.add_argument("--card", type=Path)
    destination.add_argument("--device", help="ADB transport, such as an IP address and port")
    parser.add_argument("--wifi-config", type=Path, help="Local JSON containing ssid and password")
    parser.add_argument("--restore", action="store_true", help="Restore the launcher selected before the helper")
    parser.add_argument("--adb", default=os.environ.get("ADB") or shutil.which("adb") or
                        str(Path.home() / "Library/Android/sdk/platform-tools/adb"))
    args = parser.parse_args()
    try:
        if args.device:
            deploy(args.device, args.adb, args.wifi_config, args.restore)
        else:
            install(args.card, args.wifi_config, args.restore)
    except (OSError, ValueError, subprocess.CalledProcessError):
        # Never echo command output or config contents on a credential upload failure.
        parser.exit(1, "Install failed. Check the connection, card, build and Wi-Fi config.\n")
    print("Previous launcher restored." if args.restore else "ADB Enabler installed.")
    print("UI restart requested." if args.device else "Eject the card before disconnecting it.")


if __name__ == "__main__":
    main()
