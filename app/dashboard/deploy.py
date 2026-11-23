"""Preview the cluster over ADB while preserving the SD startup loader."""
import argparse
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile
import time

from stage import ASSETS, stage


def deploy(device, adb, wifi_config=None):
    command = [adb, '-s', device]
    def run(*args):
        return subprocess.run(command + list(args), check=True, capture_output=True, timeout=10)
    def shell(script):
        return run('shell', script).stdout
    def upload(local, remote, temporary):
        pending = remote + '.next'
        shell('mkdir -p ' + shlex.quote(str(Path(remote).parent)))
        run('push', str(local), pending)
        checked = temporary / 'checked'
        run('pull', pending, str(checked))
        if checked.read_bytes() != local.read_bytes():
            raise ValueError('Upload verification failed')
        shell('mv ' + shlex.quote(pending) + ' ' + shlex.quote(remote))

    with tempfile.TemporaryDirectory() as directory:
        temporary = Path(directory)
        card = temporary / 'card'; card.mkdir()
        for name in ('EasyUI.cfg', 'Dashborne/EasyUI.previous.cfg', 'Dashborne/wifi.conf'):
            local = card / name; local.parent.mkdir(parents=True, exist_ok=True)
            result = subprocess.run(command + ['pull', '/mnt/extsd/' + name, str(local)], capture_output=True, timeout=10)
            if result.returncode and name == 'EasyUI.cfg':
                raise ValueError('Cannot read the active SD configuration')
        original = (card / 'EasyUI.cfg').read_bytes()
        stage(card, wifi_config=wifi_config)
        files = ['lib/libdashborne.so', 'ui/dashborne.ftu', 'Dashborne/EasyUI.previous.cfg']
        files += ['ui/cluster/' + name for name in ASSETS]
        if wifi_config:
            files.append('Dashborne/wifi.conf')
        for name in ('lib/libdashborne.so', 'ui/dashborne.ftu'):
            remote = '/mnt/extsd/' + name
            shell(f'if [ -f {shlex.quote(remote)} ]; then cp {shlex.quote(remote)} {shlex.quote(remote + ".previous")}; fi')
        for name in files:
            upload(card / name, '/mnt/extsd/' + name, temporary)
        before = shell('cat /mnt/extsd/Dashborne/events.log').count(b'dashborne-selected')
        restored = temporary / 'startup.cfg'; restored.write_bytes(original)
        try:
            upload(card / 'EasyUI.cfg', '/mnt/extsd/EasyUI.cfg', temporary)
            shell('sync && setprop ctl.restart zkswe')
            deadline = time.monotonic() + 40
            while time.monotonic() < deadline:
                time.sleep(1)
                try:
                    log = shell('cat /mnt/extsd/Dashborne/events.log')
                    if log.count(b'dashborne-selected') > before:
                        break
                except subprocess.SubprocessError:
                    subprocess.run([adb, 'connect', device], capture_output=True, timeout=5)
            else:
                raise ValueError('Cluster startup was not confirmed')
        finally:
            upload(restored, '/mnt/extsd/EasyUI.cfg', temporary)
            shell('sync')
    print('Cluster preview running. The previous SD startup selection is preserved.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--device', required=True)
    parser.add_argument('--wifi-config', type=Path)
    parser.add_argument('--adb', default=os.environ.get('ADB') or shutil.which('adb') or str(Path.home() / 'Library/Android/sdk/platform-tools/adb'))
    args = parser.parse_args()
    try:
        deploy(args.device, args.adb, args.wifi_config)
    except (OSError, ValueError, subprocess.SubprocessError):
        parser.exit(1, 'Preview deployment failed; check the device connection and build.\n')
