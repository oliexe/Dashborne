"""Exercise the downloadable package without the source checkout's installer."""
import hashlib
import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('release', ROOT / 'tools/release.py')
release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)


class ReleaseTests(unittest.TestCase):
    def test_download_install_upgrade_restore(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            archive = release.release(root / 'output')
            self.assertEqual((archive.parent / 'SHA256SUMS').read_text(),
                             f'{hashlib.sha256(archive.read_bytes()).hexdigest()}  {archive.name}\n')
            package = root / 'extracted'
            with zipfile.ZipFile(archive) as zipped:
                self.assertIsNone(zipped.testzip())
                zipped.extractall(package)
            bundles=package/'card/Dashborne/extensions'
            self.assertEqual({p.name for p in bundles.iterdir()}, {'widget-example','screen-mirroring'})
            self.assertEqual(list((package/'card/Dashborne/patches').glob('*.dbp')), [])
            config = release.install.configuration.parse((package / 'card/dashborne.conf').read_bytes())
            self.assertEqual(config['adb.enabled'], '0')
            self.assertEqual(config['wifi.password'], '')
            self.assertEqual(config['wifi.ssid'], '')
            card = root / 'upgrade'
            card.mkdir()
            command = [sys.executable, str(package / 'install.py'), '--card', str(card)]
            subprocess.run(command, check=True, capture_output=True)
            original = (card / release.install.BACKUP).read_bytes()
            preferences = b'format=1\nadb.enabled=0\nfuture.option=keep\n'
            (card / 'dashborne.conf').write_bytes(preferences)
            subprocess.run(command, check=True, capture_output=True)
            self.assertEqual((card / 'dashborne.conf').read_bytes(), preferences)
            self.assertEqual((card / release.install.BACKUP).read_bytes(), original)
            subprocess.run(command + ['--restore'], check=True, capture_output=True)
            self.assertEqual((card / 'EasyUI.cfg').read_bytes(), original)

    def test_tag_must_match_version(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(ValueError, 'tag must match'):
                release.release(directory, tag='v999.999.999')
