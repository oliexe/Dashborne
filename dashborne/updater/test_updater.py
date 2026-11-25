"""Exercise the real C updater in isolated cards, including process-death recovery."""
import base64
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
import zipfile

HERE = Path(__file__).resolve().parent
BINARY = Path(os.environ.get('UPDATER_TEST_BINARY', HERE.parent / 'build/updater-test'))


class UpdaterTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.card = Path(self.temp.name).resolve()
        self.active = self.card / 'Dashborne'
        self.work = self.card / '.dashborne-update'
        self.stage = self.work / 'stage'
        self.startup = b'{"startupLibPath":"/mnt/extsd/Dashborne/lib/libdashborne.so","touchDev":"/dev/input/event0"}'
        self.config = b'wifi.password=private\nloader.autolaunch=1\n'
        self.put(self.card / 'EasyUI.cfg', self.startup)
        self.put(self.card / 'dashborne.conf', self.config)
        for name, data in {'lib/libdashborne.so': b'old', 'bin/updater': b'old helper', 'lib/update-recovery.so': b'shim', 'previous.cfg': b'original startup', 'extensions/custom/main.so': b'community', 'patches/catalog.tsv': b'custom\tCustom\tDescription\tnative\n'}.items():
            self.put(self.active / name, data)
        for name, data in {'lib/libdashborne.so': b'new', 'bin/updater': b'new helper', 'lib/update-recovery.so': b'new shim'}.items():
            self.put(self.stage / name, data)

    def put(self, p, data):
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_bytes(data)

    def run_updater(self, *args, ok=True, **env):
        options = dict(os.environ, UPDATER_TEST_CARD=str(self.card), UPDATER_TEST_OS='6.11.4', ASAN_OPTIONS='detect_leaks=0', **env)
        result = subprocess.run([str(BINARY), *map(str, args)], env=options, capture_output=True, text=True)
        self.assertNotIn('AddressSanitizer', result.stderr)
        self.assertNotIn('runtime error:', result.stderr)
        if ok:
            self.assertEqual(result.returncode, 0, result.stderr)
        else:
            self.assertNotEqual(result.returncode, 0)
        return result

    def manifest(self, files, version='0.2.0'):
        return {'format': 1, 'version': version, 'stock_os': '6.11.4', 'files': {p: hashlib.sha256(data).hexdigest() for p, data in files.items()}}

    def archive(self, extra=None, corrupt=False, version='0.2.0', compression=zipfile.ZIP_DEFLATED):
        elf = bytearray(52)
        elf[:6] = b'\x7fELF\x01\x01'
        elf[18] = 40
        files = {'lib/libdashborne.so': bytes(elf), 'ui/loader.ftu': b'layout', 'bin/updater': b'helper', 'lib/update-recovery.so': b'shim', 'patches/catalog.tsv': b'bundled\tBundled\tNew\tnative\n'}
        manifest = self.manifest(files, version)
        if corrupt:
            files['ui/loader.ftu'] = b'wrong'
        files['update-manifest.json'] = json.dumps(manifest).encode()
        path = self.card / 'release.zip'
        with zipfile.ZipFile(path, 'w', compression=compression) as archive:
            for p, data in files.items():
                archive.writestr('card/Dashborne/' + p, data)
            if extra:
                archive.writestr(extra, b'bad')
        return path

    def assert_original(self):
        self.assertEqual((self.active / 'lib/libdashborne.so').read_bytes(), b'old')
        self.assertEqual((self.card / 'EasyUI.cfg').read_bytes(), self.startup)
        self.assertEqual((self.card / 'dashborne.conf').read_bytes(), self.config)

    def test_process_death_recovery(self):
        for step in ['armed', 'old-moved', 'new-moved', 'installed']:
            with self.subTest(step=step):
                # Each iteration starts with a fresh fixture.
                if step != 'armed':
                    self.tearDown()
                    self.setUp()
                result = self.run_updater('apply', self.stage, ok=False, UPDATER_TEST_FAULT=step)
                self.assertEqual(result.returncode, 90)
                self.run_updater('recover')
                if step == 'installed':
                    self.assertEqual((self.active / 'lib/libdashborne.so').read_bytes(), b'new')
                    self.run_updater('recover')
                self.assert_original()
                self.run_updater('recover')
                self.assertFalse((self.work / 'journal.json').exists())

    def test_confirmed_update_and_legacy_journal(self):
        self.run_updater('apply', self.stage)
        j = json.loads((self.work / 'journal.json').read_text())
        # Same Phase/Startup schema and base64 bytes as the previous updater.
        self.assertEqual(base64.b64decode(j['Startup']), self.startup)
        self.run_updater('confirm', ok=False)
        self.run_updater('recover')
        self.run_updater('confirm')
        self.run_updater('recover')
        self.assertEqual((self.active / 'lib/libdashborne.so').read_bytes(), b'new')
        self.assertEqual((self.work / 'previous/lib/libdashborne.so').read_bytes(), b'old')
        self.assertEqual((self.card / 'EasyUI.cfg').read_bytes(), self.startup)
        self.assertFalse((self.work / 'journal.json').exists())

    def test_preserve_community(self):
        self.put(self.active / 'extensions/retired/main.so', b'old')
        self.put(self.active / 'update-manifest.json', json.dumps(self.manifest({'extensions/retired/main.so': b'old'})).encode())
        self.put(self.stage / 'patches/catalog.tsv', b'bundled\tBundled\tNew\tnative\n')
        self.put(self.stage / 'previous.cfg', b'default')
        self.put(self.active / 'licenses/updater/GO-LICENSE', b'obsolete')
        self.put(self.stage / 'update-manifest.json', json.dumps(self.manifest({'extensions/bundled/main.so': b'new'})).encode())
        self.run_updater('preserve', self.stage)
        self.assertTrue((self.stage / 'extensions/custom/main.so').exists())
        self.assertFalse((self.stage / 'extensions/retired').exists())
        self.assertIn(b'custom\t', (self.stage / 'patches/catalog.tsv').read_bytes())
        self.assertEqual((self.stage / 'previous.cfg').read_bytes(), b'original startup')
        self.assertFalse((self.stage / 'licenses/updater/GO-LICENSE').exists())
        self.assertEqual((self.card / 'dashborne.conf').read_bytes(), self.config)

    def test_archives(self):
        cases = ['card/Dashborne/../escape', '/absolute', 'card/Dashborne/ui/LOADER.ftu', 'card/Dashborne/UI/another', 'card/Dashborne/unlisted', 'card/Dashborne/bad\\path', 'card/Dashborne/trailing.', 'card/Dashborne/ui']
        for extra in cases:
            with self.subTest(extra=extra):
                dest = self.card / 'extract'
                shutil.rmtree(dest, ignore_errors=True)
                self.run_updater('extract', self.archive(extra), dest, '0.2.0', '6.11.4', ok=False)
        for corrupt, os_version, compression in [(True, '6.11.4', zipfile.ZIP_DEFLATED), (False, '6.11.2', zipfile.ZIP_DEFLATED), (False, '6.11.4', zipfile.ZIP_DEFLATED), (False, '6.11.4', zipfile.ZIP_STORED)]:
            dest = self.card / 'extract'
            shutil.rmtree(dest, ignore_errors=True)
            self.run_updater('extract', self.archive(corrupt=corrupt, compression=compression), dest, '0.2.0', os_version, ok=not corrupt and os_version == '6.11.4')
        z = self.archive()
        link = zipfile.ZipInfo('card/Dashborne/link')
        link.create_system = 3
        link.external_attr = 0o120777 << 16
        with zipfile.ZipFile(z, 'a') as archive:
            archive.writestr(link, '/etc/passwd')
        shutil.rmtree(dest, ignore_errors=True)
        self.run_updater('extract', z, dest, '0.2.0', '6.11.4', ok=False)

    def test_url_and_utf8(self):
        for url in ['http://github.com/a', 'https://github.com.evil/a', 'https://user@github.com/a', 'https://github.com:443/a', 'file:///etc/passwd', 'https://github.com/\r\nevil', 'https://github.com\\@evil/a']:
            self.run_updater('url', url, ok=False)
        self.run_updater('url', 'https://release-assets.githubusercontent.com/a')
        self.assertEqual(self.run_updater('text', 'αβ\nword').stdout.strip(), 'α')

    def releases(self, size, supported=True):
        assets = [{'name': 'Dashborne-0.2.0-microSD.zip', 'browser_download_url': 'https://github.com/archive', 'size': size}, {'name': 'SHA256SUMS', 'browser_download_url': 'https://github.com/checksum'}]
        if supported:
            assets.append({'name': 'updater-v1.json'})
        return [{'id': 1, 'tag_name': 'v0.2.0', 'name': 'Dashborne 0.2.0 (Stock OS 6.11.4)', 'body': 'Release\nnotes', 'assets': assets}, {'id': 2, 'draft': True, 'tag_name': 'v0.3.0', 'name': 'Dashborne 0.3.0 (Stock OS 6.11.4)'}]

    def test_download_and_rollback(self):
        http = self.card / 'http'
        archive = self.archive().read_bytes()
        self.put(http / 'archive', archive)
        self.put(http / 'sum', (hashlib.sha256(archive).hexdigest() + '  Dashborne-0.2.0-microSD.zip\n').encode())
        self.put(http / 'releases', json.dumps(self.releases(len(archive))).encode())
        self.run_updater('list', UPDATER_TEST_HTTP=str(http))
        self.assertEqual((self.card / 'list.tsv').read_text(), '1\t0.2.0\t6.11.4\ttrue\tRelease notes\n')
        self.run_updater('install', '1', UPDATER_TEST_HTTP=str(http))
        self.run_updater('recover')
        self.run_updater('confirm')
        self.assertTrue((self.active / 'extensions/custom/main.so').exists())
        # A second valid install makes the first updater-compatible install the backup.
        self.run_updater('install', '1', UPDATER_TEST_HTTP=str(http))
        self.run_updater('recover')
        self.run_updater('confirm')
        self.run_updater('rollback')
        self.run_updater('recover')
        self.run_updater('confirm')
        self.assertTrue((self.active / 'extensions/custom/main.so').exists())
        self.assertEqual((self.card / 'dashborne.conf').read_bytes(), self.config)

    def test_download_failures_leave_active_untouched(self):
        http = self.card / 'http'
        archive = self.archive().read_bytes()
        for fault in ['checksum', 'short', 'HTTP', 'OS', 'legacy', 'digest']:
            with self.subTest(fault=fault):
                self.put(http / 'archive', archive[:len(archive)//2] if fault == 'short' else archive)
                checksum = '0' * 64 if fault == 'checksum' else hashlib.sha256(archive).hexdigest()
                self.put(http / 'sum', (checksum + '  Dashborne-0.2.0-microSD.zip\n').encode())
                releases = self.releases(len(archive), fault != 'legacy')
                if fault == 'OS':
                    releases[0]['name'] = 'Dashborne 0.2.0 (Stock OS 6.11.2)'
                if fault == 'digest':
                    releases[0]['assets'][0]['digest'] = 'sha256:' + '0' * 64
                self.put(http / 'releases', json.dumps(releases).encode())
                self.run_updater('list', UPDATER_TEST_HTTP=str(http))
                if fault == 'HTTP':
                    (http / 'archive').unlink()
                self.run_updater('install', '1', ok=False, UPDATER_TEST_HTTP=str(http))
                self.assert_original()
                self.assertFalse((self.work / 'journal.json').exists())

    def test_lock_and_invalid_journal(self):
        import fcntl
        self.work.mkdir(exist_ok=True)
        with (self.work / 'lock').open('w') as lock:
            fcntl.flock(lock, fcntl.LOCK_EX)
            self.run_updater('recover', ok=False)
        self.put(self.work / 'journal.json', b'{"Phase":"unknown","Startup":"x"}')
        self.run_updater('recover', ok=False)
        self.assert_original()


if __name__ == '__main__':
    unittest.main()
