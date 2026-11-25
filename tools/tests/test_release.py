"""Exercise the downloadable package without the source checkout's installer."""
import hashlib
import json
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
            version = (ROOT / 'VERSION').read_text().strip()
            stock_os = (ROOT / 'STOCK_OS_VERSION').read_text().strip()
            self.assertEqual((archive.parent / 'release-title.txt').read_text(),
                             f'Dashborne {version} (Stock OS {stock_os})\n')
            self.assertEqual((archive.parent / 'SHA256SUMS').read_text(),
                             f'{hashlib.sha256(archive.read_bytes()).hexdigest()}  {archive.name}\n')
            package = root / 'extracted'
            with zipfile.ZipFile(archive) as zipped:
                self.assertIsNone(zipped.testzip())
                zipped.extractall(package)
            self.assertEqual(json.loads((archive.parent/'updater-v1.json').read_text()), {'format': 1})
            manifest = json.loads((package/'card/Dashborne/update-manifest.json').read_text())
            self.assertEqual((manifest['format'], manifest['version'], manifest['stock_os']), (1, version, stock_os))
            for name, digest in manifest['files'].items():
                self.assertEqual(hashlib.sha256((package/'card/Dashborne'/name).read_bytes()).hexdigest(), digest)
            self.assertIn('bin/updater', manifest['files'])
            self.assertEqual((package / 'STOCK_OS_VERSION').read_text().strip(), stock_os)
            self.assertIn(f'Target stock OS: {stock_os}.', (package / 'README.md').read_text())
            bundles=package/'card/Dashborne/extensions'
            for size in ('1x1','2x1','1x2','2x2','3x2','4x2','5x2','6x2'):
                for folder in (package/'card/Dashborne/assets/widget-runtime',
                               bundles/'widget-example/assets'):
                    self.assertTrue((folder/f'preview-{size}.png').read_bytes().startswith(b'\x89PNG\r\n\x1a\n'))
            self.assertEqual({p.name for p in bundles.iterdir()}, {'widget-example','screen-mirroring','tesla-model','development-menu','tesla-skin','better-display-controls'})
            self.assertFalse((bundles/'tesla-model/models').exists())
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
