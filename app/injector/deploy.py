"""Stage a patch-only injector and preview it with a two-minute recovery timer."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent
REMOTE = '/mnt/extsd/Dashborne/injector'
LIBRARY = '/mnt/extsd/lib/libdashborne-injector.so'
TARGET = '/res/ui/dash1act.ftu'
CONFIRMED = '/tmp/dashborne-injector-confirmed'
DECISION = '/tmp/dashborne-injector-decision'
RESTORE = f'''#!/bin/sh
set -eu
trap '' HUP
cp {REMOTE}/startup.cfg /mnt/extsd/EasyUI.cfg
sync
# Reboot clears the bind mount and the official app's network/display state.
reboot
'''
START = f'''#!/bin/sh
set -eu
trap '' HUP
rm -f {CONFIRMED} {REMOTE}/disabled
rmdir {DECISION} 2>/dev/null || true
( ping -c 121 127.0.0.1 >/dev/null; if mkdir {DECISION} 2>/dev/null; then sh {REMOTE}/restore.sh; fi ) </dev/null >{REMOTE}/recovery.log 2>&1 &
( ping -c 46 127.0.0.1 >/dev/null; /res/bin/fbgrab -a -d /dev/fb0 {REMOTE}/preview.png; cat /proc/net/route; getprop init.svc.adbd ) </dev/null >{REMOTE}/capture.log 2>&1 &
cp {REMOTE}/active.cfg /mnt/extsd/EasyUI.cfg
sync
setprop ctl.restart zkswe
ping -c 3 127.0.0.1 >/dev/null
cp {REMOTE}/startup.cfg /mnt/extsd/EasyUI.cfg
sync
'''
ACTIVATE = f'''#!/bin/sh
set -eu
trap '' HUP
rm -f {REMOTE}/disabled
cp {REMOTE}/active.cfg /mnt/extsd/EasyUI.cfg
sync
setprop ctl.restart zkswe
'''


def configuration(raw):
    config = json.loads(raw)
    if config.get('resPath') != '/res/ui/':
        raise ValueError('Expected the installed /res/ui/ resource root')
    if config.get('startupLibPath') == LIBRARY:
        raise ValueError('Injector already selected; restore before another preview')
    config['startupLibPath'] = LIBRARY
    return (json.dumps(config, indent=2) + '\n').encode()


def deploy(device, adb, action):
    command = [adb, '-s', device]
    def run(*args):
        return subprocess.run(command + list(args), check=True, capture_output=True, timeout=15)
    def shell(text):
        return run('shell', text).stdout
    if action == 'keep':
        # Persist the working card selection only after explicit visual verification.
        if not any(line.split()[1] == TARGET for line in shell('cat /proc/mounts').decode().splitlines()):
            raise ValueError('No active patch preview to keep')
        shell(f'test -f {REMOTE}/active.cfg && mkdir {DECISION} && touch {CONFIRMED} && cp {REMOTE}/active.cfg /mnt/extsd/EasyUI.cfg && sync')
        print('Preview kept; the microSD now selects the injector at startup.')
        return
    if action == 'restore':
        shell(f'mkdir {DECISION} 2>/dev/null || true; touch {CONFIRMED}; sh {REMOTE}/restore.sh')
        print('Selected the previous SD app and requested a reboot to clear the override.')
        return
    with tempfile.TemporaryDirectory() as directory:
        temporary = Path(directory)
        def upload(data, remote):
            source, checked = temporary / 'upload', temporary / 'checked'
            source.write_bytes(data)
            pending = remote + '.next'
            run('push', str(source), pending)
            run('pull', pending, str(checked))
            if checked.read_bytes() != data:
                raise ValueError('Upload readback did not match')
            shell('mv ' + shlex.quote(pending) + ' ' + shlex.quote(remote))
        mounts = shell('cat /proc/mounts').decode()
        if any(line.split()[1] in ('/res', TARGET) and
               ('/mnt/extsd/' in line or line.split()[1] == TARGET)
               for line in mounts.splitlines()):
            raise ValueError('Restore the previous resource override before previewing')
        run('pull', '/mnt/extsd/EasyUI.cfg', str(temporary / 'startup.cfg'))
        raw = (temporary / 'startup.cfg').read_bytes()
        active = configuration(raw)
        # Validate compatibility on the laptop before changing the card selection.
        run('pull', TARGET, str(temporary / 'base.ftu'))
        import hashlib
        from package import compile_patch
        spec = json.loads((ROOT / 'patches/badge.json').read_text())
        if hashlib.sha256((temporary / 'base.ftu').read_bytes()).hexdigest() != spec['base']['sha256']:
            raise ValueError('Installed layout does not match this patch version')
        package = (ROOT / 'build/badge.dbp').read_bytes()
        if package != compile_patch(spec):
            raise ValueError('Patch package is stale; rebuild it')
        shell(f'mkdir -p {REMOTE} /mnt/extsd/lib')
        for data, remote in (
            ((ROOT / 'build/libdashborne-injector.so').read_bytes(), LIBRARY),
            (package, REMOTE + '/badge.dbp'),
            (raw, REMOTE + '/startup.cfg'), (active, REMOTE + '/active.cfg'),
            (RESTORE.encode(), REMOTE + '/restore.sh'),
            ((ACTIVATE if action == 'activate' else START).encode(), REMOTE + '/start.sh'),
        ):
            upload(data, remote)
        shell(f'sh {REMOTE}/start.sh')
    print('Injector selected for microSD startup.' if action == 'activate' else
          'Preview started. It returns to the previous SD app in two minutes unless --keep is used.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--device', required=True)
    actions = parser.add_mutually_exclusive_group()
    actions.add_argument('--keep', action='store_true')
    actions.add_argument('--restore', action='store_true')
    actions.add_argument('--activate', action='store_true', help='Select the verified injector at startup without a preview timer')
    parser.add_argument('--adb', default=os.environ.get('ADB') or shutil.which('adb') or
                        str(Path.home() / 'Library/Android/sdk/platform-tools/adb'))
    args = parser.parse_args()
    try:
        deploy(args.device, args.adb, 'keep' if args.keep else 'restore' if args.restore else
               'activate' if args.activate else 'preview')
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f'Injector deployment failed ({type(error).__name__}). Check connection, build and installed version.\n')
