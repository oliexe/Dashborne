"""Host-side reader/writer for the root-card Dashborne configuration."""
import re
from pathlib import Path

LIMIT = 32768
KEY = re.compile(r'[a-zA-Z0-9_.-]{1,95}')
LEGACY = ('Dashborne/settings.conf', 'Dashborne/wifi.conf')


def parse(raw):
    if len(raw) > LIMIT or b'\0' in raw or b'\r' in raw.replace(b'\r\n', b''):
        raise ValueError('Invalid configuration')
    result = {}
    for line in raw.decode('utf-8').split('\n'):
        line = line.removesuffix('\r').lstrip(' \t')
        if not line or line.startswith(('#', ';')):
            continue
        key, separator, value = line.partition('=')
        key = key.rstrip(' \t')
        if not separator or not KEY.fullmatch(key) or len(value.encode()) >= 1024 or key in result:
            raise ValueError('Invalid configuration')
        result[key] = value
    if result.get('format') != '1':
        raise ValueError('Unsupported configuration format')
    return result


def update(raw, changes):
    parse(raw)
    pending = dict(changes)
    for key, value in pending.items():
        if not KEY.fullmatch(key) or any(c in value for c in '\r\n\0') or len(value.encode()) >= 1024:
            raise ValueError('Invalid setting')
    lines = []
    chunks=raw.decode('utf-8').split('\n')
    for index, chunk in enumerate(chunks):
        line=chunk+('\n' if index<len(chunks)-1 else '')
        key = line.lstrip(' \t').partition('=')[0].rstrip(' \t')
        if key in pending:
            lines.append(key + '=' + pending.pop(key) + '\n')
        else:
            lines.append(line)
    output = ''.join(lines)
    if pending and output and not output.endswith('\n'):
        output += '\n'
    output += ''.join(key+'='+value+'\n' for key, value in pending.items())
    encoded = output.encode('utf-8')
    parse(encoded)
    return encoded


def wifi(raw):
    # Legacy files use exactly two lines, with optional CRLF/final newline.
    if len(raw) >= 100 or b'\0' in raw:
        raise ValueError('Invalid Wi-Fi configuration')
    lines = raw.replace(b'\r\n', b'\n').removesuffix(b'\n').split(b'\n')
    if len(lines) != 2 or b'\r' in raw.replace(b'\r\n', b''):
        raise ValueError('Invalid Wi-Fi configuration')
    ssid, password = (x.decode('utf-8') for x in lines)
    if not 1 <= len(lines[0]) <= 32 or not 8 <= len(lines[1]) <= 63:
        raise ValueError('Invalid Wi-Fi configuration')
    return {'wifi.ssid': ssid, 'wifi.password': password}


def migrate(card):
    card = Path(card)
    target = card/'dashborne.conf'
    if target.exists():
        raw = target.read_bytes()
        parse(raw)
        return raw
    changes = {'adb.enabled': '0', 'wifi.ssid': '', 'wifi.password': '',
               'loader.autolaunch': '0', 'loader.app': ''}
    settings = card/LEGACY[0]
    if settings.exists():
        raw = settings.read_bytes()
        if len(raw) > 8192 or b'\0' in raw:
            raise ValueError('Invalid legacy settings')
        for line in raw.decode('utf-8').splitlines():
            key, _, value = line.partition('=')
            if value not in ('0', '1'):
                continue
            if key == 'wifi_adb':
                changes['adb.enabled'] = value
            elif key.startswith('patch.') and re.fullmatch('[a-z0-9_-]{1,31}', key[6:]):
                changes['tweak.'+key[6:]+'.enabled'] = value
    for name in (LEGACY[1], 'adb-enabler/wifi.conf'):
        source = card/name
        if source.exists():
            changes.update(wifi(source.read_bytes()))
            break
    return update(b'format=1\n', changes)
