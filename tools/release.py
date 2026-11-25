"""Package a completed build for microSD installation and GitHub Releases."""
import argparse
import hashlib
from pathlib import Path
import re
import shutil
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'dashborne'))
import install
import update_manifest

INSTRUCTIONS = """# Install Dashborne {version}

Target stock OS: {stock_os}.

1. Prepare a microSD card with an MBR partition table and a FAT32 partition.
   An 8 GB FAT32 partition has been tested. Formatting erases the card.
2. For a fresh card, copy the contents of `card/` to its root:
   `Dashborne/`, `EasyUI.cfg` and `dashborne.conf`.
   To upgrade an existing card, use Python 3.10+ from this extracted folder:
   `python3 install.py --card /path/to/card`
   The installer preserves settings and the previous startup configuration.
3. Eject the card, insert it into the powered-off DASH, then power on.
   Tap the splash within three seconds to open the boot menu.

ADB is off by default. Set up Wi-Fi from the boot menu, then enable ADB when needed.
Use Update to choose a compatible GitHub release and install it on the card.
Power off and remove the card to boot without Dashborne.

Documentation: https://github.com/oliexe/Dashborne/tree/v{version}
"""


def release(output, tag=None):
    version = (ROOT / 'VERSION').read_text().strip()
    if not re.fullmatch(r'[0-9]+\.[0-9]+\.[0-9]+', version):
        raise ValueError('VERSION must contain a release version, such as 0.1.0')
    if tag is not None and tag != 'v' + version:
        raise ValueError('Release tag must match VERSION')
    stock_os = (ROOT / 'STOCK_OS_VERSION').read_text().strip()
    if not re.fullmatch(r'[0-9]+\.[0-9]+\.[0-9]+', stock_os):
        raise ValueError('STOCK_OS_VERSION must contain a stock OS version, such as 6.11.4')
    changelog = (ROOT / 'CHANGELOG.md').read_text()
    match = re.search(r'^## ' + re.escape(version) + r' — [^\n]+\n(.*?)(?=^## |\Z)',
                      changelog, re.M | re.S)
    if not match or not match[1].strip():
        raise ValueError('Missing changelog entry for VERSION')
    output = Path(output)
    output.mkdir(parents=True, exist_ok=True)
    archive = output / f'Dashborne-{version}-microSD.zip'
    with tempfile.TemporaryDirectory() as temporary:
        package = Path(temporary)
        card = package / 'card'
        card.mkdir()
        install.stage(card)
        update_manifest.write(card / "Dashborne")
        for name in ('install.py', 'configuration.py', 'files.py'):
            shutil.copyfile(ROOT / 'dashborne' / name, package / name)
        for name in ('VERSION', 'STOCK_OS_VERSION', 'CHANGELOG.md'):
            shutil.copyfile(ROOT / name, package / name)
        (package / 'README.md').write_text(INSTRUCTIONS.format(version=version, stock_os=stock_os))
        with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as zipped:
            for path in sorted(package.rglob('*')):
                if path.is_file():
                    zipped.write(path, path.relative_to(package))
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    (output / 'updater-v1.json').write_text('{"format":1}\n')
    (output / 'SHA256SUMS').write_text(f'{digest}  {archive.name}\n')
    notes = match[1].strip() + '\n\n' + (
        f'Download **{archive.name}** below and follow its `README.md`. '
        'Use the installer when upgrading to preserve your settings. '
        '`SHA256SUMS` contains the archive checksum.\n')
    (output / 'release-notes.md').write_text(notes)
    (output / 'release-title.txt').write_text(f'Dashborne {version} (Stock OS {stock_os})\n')
    return archive


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'dashborne/build/release')
    parser.add_argument('--tag')
    args = parser.parse_args()
    print(release(args.output, args.tag))
