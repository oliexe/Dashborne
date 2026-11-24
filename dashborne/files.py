"""Host file writes and Wi-Fi input validation for card installation."""
import json
import os
from pathlib import Path


def atomic_write(path, content):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".tmp")
    with temporary.open("wb") as stream:
        stream.write(content)
        stream.flush()
        os.fsync(stream.fileno())
    temporary.replace(path)


def wifi_config_bytes(path):
    config = json.loads(Path(path).read_bytes())
    ssid, password = config.get("ssid"), config.get("password")
    if not isinstance(ssid, str) or not isinstance(password, str):
        raise ValueError("Wi-Fi config needs ssid and password strings")
    if (not 1 <= len(ssid.encode()) <= 32 or not 8 <= len(password.encode()) <= 63 or
            any(c in ssid + password for c in "\r\n\0")):
        raise ValueError("Invalid Wi-Fi SSID or WPA passphrase length/characters")
    return (ssid + "\n" + password + "\n").encode()
