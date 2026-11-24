"""Install Dashborne's splash, configuration and patch loader on microSD."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT))
import configuration
from files import atomic_write, wifi_config_bytes
STARTUP = '/mnt/extsd/Dashborne/lib/libdashborne.so'
BACKUP = 'Dashborne/previous.cfg'
# Startup fields for the tested DASH layout; existing card fields take precedence.
DEFAULT_STARTUP = {
    'startupLibPath': '/res/lib/libzkgui.so', 'resPath': '/res/ui/',
    'rotateScreen': 270, 'rotateTouch': 0, 'touchDev': '/dev/input/event1',
    'font': '/res/font/a.ttf:/res/font/akshar.ttf:/res/font/digits.ttf:/res/font/inter.ttf',
    'languageCode': 'en_US', 'languagePath': '/res/tr/',
    'uart': 'ttyS1', 'baud': '115200', 'defBrightness': -1,
    'screensaverTimeOut': -1, 'startupTouchCalib': False, 'zkdebug': False,
}
SAVED = ('dashborne.conf', 'EasyUI.cfg', BACKUP, 'Dashborne/settings.conf', 'Dashborne/wifi.conf', 'adb-enabler/wifi.conf')
RETIRED_CORE = ('Dashborne/extensions/extra-menu', 'Dashborne/extensions/widget-runtime')
RETIRED_PACKAGES = RETIRED_CORE + ('Dashborne/extensions/tesla-ui',
                                 'Dashborne/extensions/vehicle-widget',
                                 'Dashborne/extensions/contrast')
RETIRED_FILES = ('Dashborne/patches/dashborne-badge.dbp',)


def stage(card, wifi_config=None, configure=False, restore=False, build=None):
    card = Path(card)
    if not card.is_dir():
        raise ValueError('Card directory must already exist')
    startup = card / 'EasyUI.cfg'
    raw = (startup.read_bytes() if startup.exists() or restore else
           (json.dumps(DEFAULT_STARTUP, indent=2) + '\n').encode())
    config = json.loads(raw)
    if restore:
        if config.get('startupLibPath') != STARTUP:
            raise ValueError('Dashborne is not selected')
        previous = (card / BACKUP).read_bytes(); json.loads(previous)
        atomic_write(card / 'EasyUI.cfg', previous)
        return ['EasyUI.cfg']
    if config.get('resPath') != '/res/ui/':
        raise ValueError('Expected the installed resource path')
    unified = configuration.migrate(card)
    if wifi_config:
        unified = configuration.update(unified, configuration.wifi(wifi_config_bytes(wifi_config)))
    build = Path(build) if build else (ROOT / 'card' if (ROOT / 'card').is_dir() else ROOT / 'build/card')
    library = (build / 'Dashborne/lib/libdashborne.so').read_bytes()
    if len(library) < 52 or library[:6] != b'\x7fELF\x01\x01' or library[18:20] != b'\x28\x00':
        raise ValueError('Build the ARM loader first')
    for name in ('menu.ftu', 'dashboard.dbp', 'star.png', 'star-pressed.png', 'tweaks.png'):
        if not (build / 'Dashborne/assets/extra-menu' / name).is_file():
            raise ValueError('Core menu assets are missing from the build')
    if any((build / legacy).exists() for legacy in RETIRED_CORE):
        raise ValueError('Rebuild Dashborne before migrating the core services')
    changed = []
    if config.get('startupLibPath') != STARTUP:
        atomic_write(card / BACKUP, raw); changed.append(BACKUP)
    elif not (card / BACKUP).exists():
        raise ValueError('Previous startup configuration missing')
    for source in sorted(build.rglob('*')):
        if source.is_file():
            name = str(source.relative_to(build))
            if name in SAVED: continue
            atomic_write(card / name, source.read_bytes()); changed.append(name)
    if not (card/'dashborne.conf').exists() or (card/'dashborne.conf').read_bytes() != unified:
        atomic_write(card/'dashborne.conf', unified); changed.append('dashborne.conf')
    for legacy in configuration.LEGACY:
        (card/legacy).unlink(missing_ok=True)
    for legacy in RETIRED_PACKAGES:
        if (build / legacy).exists(): continue
        folder = card / legacy
        if folder.is_symlink(): folder.unlink()
        elif folder.is_dir(): shutil.rmtree(folder)
    for legacy in RETIRED_FILES:
        if not (build / legacy).exists(): (card / legacy).unlink(missing_ok=True)
    if configure:
        atomic_write(card / 'Dashborne/config.once', b''); changed.append('Dashborne/config.once')
    config['startupLibPath'] = STARTUP
    atomic_write(card / 'EasyUI.cfg', (json.dumps(config, indent=2) + '\n').encode())
    return changed + ['EasyUI.cfg']


def deploy(device, adb, wifi_config=None, configure=False, restore=False, preview=False, keep=False):
    command = [adb, '-s', device]
    def run(*args):
        return subprocess.run(command + list(args), check=True, capture_output=True, timeout=20)
    def shell(script):
        return run('shell', script).stdout
    if keep:
        shell('touch /tmp/dashborne-loader-confirmed && cp /mnt/extsd/Dashborne/active.cfg /mnt/extsd/EasyUI.cfg && sync')
        print('Dashborne selected for microSD startup.'); return
    with tempfile.TemporaryDirectory() as directory:
        temporary = Path(directory); card = temporary / 'card'; card.mkdir()
        for name in SAVED:
            local = card / name; local.parent.mkdir(parents=True, exist_ok=True)
            r = subprocess.run(command + ['pull', '/mnt/extsd/' + name, str(local)], capture_output=True, timeout=10)
            if r.returncode:
                if name == 'EasyUI.cfg' or b'No such file' not in r.stderr+r.stdout and b'does not exist' not in r.stderr+r.stdout:
                    raise ValueError('Cannot read existing card configuration')
        original = (card / 'EasyUI.cfg').read_bytes()
        changed = stage(card, wifi_config, configure, restore)
        def upload(data, remote):
            source, checked = temporary / 'upload', temporary / 'checked'
            source.write_bytes(data); pending = remote + '.next'
            shell('mkdir -p ' + shlex.quote(str(Path(remote).parent)))
            run('push', str(source), pending); run('pull', pending, str(checked))
            if checked.read_bytes() != data: raise ValueError('Upload verification failed')
            shell('mv ' + shlex.quote(pending) + ' ' + shlex.quote(remote))
        for name in changed:
            if name != 'EasyUI.cfg': upload((card / name).read_bytes(), '/mnt/extsd/' + name)
        if not restore:
            # Root configuration is verified before retiring the old active files.
            for legacy in configuration.LEGACY:
                shell('rm -f '+shlex.quote('/mnt/extsd/'+legacy))
            for legacy in RETIRED_PACKAGES:
                if not (card / legacy).exists(): shell('rm -rf '+shlex.quote('/mnt/extsd/'+legacy))
            for legacy in RETIRED_FILES:
                if not (card / legacy).exists(): shell('rm -f '+shlex.quote('/mnt/extsd/'+legacy))
        if preview:
            before = shell('if [ -f /mnt/extsd/Dashborne/loader.log ]; then cat /mnt/extsd/Dashborne/loader.log; fi').count(b'dashborne-selected')
            upload(original, '/mnt/extsd/Dashborne/preview-previous.cfg')
            upload((card / 'EasyUI.cfg').read_bytes(), '/mnt/extsd/Dashborne/active.cfg')
            script = '''#!/bin/sh
set -eu
trap '' HUP
rm -f /tmp/dashborne-loader-confirmed
( ping -c 241 127.0.0.1 >/dev/null; if [ ! -f /tmp/dashborne-loader-confirmed ]; then cp /mnt/extsd/Dashborne/preview-previous.cfg /mnt/extsd/EasyUI.cfg; sync; reboot; fi ) </dev/null >/mnt/extsd/Dashborne/recovery.log 2>&1 &
cp /mnt/extsd/Dashborne/active.cfg /mnt/extsd/EasyUI.cfg
sync
setprop ctl.restart zkswe
ping -c 3 127.0.0.1 >/dev/null
cp /mnt/extsd/Dashborne/preview-previous.cfg /mnt/extsd/EasyUI.cfg
sync
'''
            upload(script.encode(), '/mnt/extsd/Dashborne/preview.sh')
            try:
                shell('sh /mnt/extsd/Dashborne/preview.sh')
            except subprocess.TimeoutExpired:
                # Some adbd builds keep the shell transport attached to the
                # background recovery timer. Verify the launcher before reporting.
                log = shell('cat /mnt/extsd/Dashborne/loader.log')
                if log.count(b'dashborne-selected') <= before:
                    raise
        else:
            upload((card / 'EasyUI.cfg').read_bytes(), '/mnt/extsd/EasyUI.cfg')
            shell('sync; ' + ('reboot' if restore else 'setprop ctl.restart zkswe'))
    print('Dashborne preview started; recovery reboots in four minutes unless kept.' if preview else
          'Previous startup restored; reboot requested.' if restore else 'Dashborne installed and startup requested.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    destination = parser.add_mutually_exclusive_group(required=True)
    destination.add_argument('--card', type=Path)
    destination.add_argument('--device')
    parser.add_argument('--wifi-config', type=Path)
    parser.add_argument('--configure', action='store_true', help='Open configuration once instead of showing the countdown')
    actions = parser.add_mutually_exclusive_group()
    actions.add_argument('--restore', action='store_true')
    actions.add_argument('--preview', action='store_true')
    actions.add_argument('--keep', action='store_true')
    parser.add_argument('--adb', default=os.environ.get('ADB') or shutil.which('adb') or
                        str(Path.home() / 'Library/Android/sdk/platform-tools/adb'))
    args = parser.parse_args()
    if args.card and (args.preview or args.keep): parser.error('--preview and --keep require --device')
    try:
        if args.device:
            deploy(args.device, args.adb, args.wifi_config, args.configure, args.restore, args.preview, args.keep)
        else:
            stage(args.card, args.wifi_config, args.configure, args.restore)
            print('Card ready. Eject it before disconnecting.')
    except (OSError, ValueError, subprocess.SubprocessError) as e:
        parser.exit(1, 'Install failed (' + type(e).__name__ + '). Check the connection, card and build.\n')
