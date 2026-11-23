import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('loader_install',ROOT/'install.py')
install=importlib.util.module_from_spec(spec);spec.loader.exec_module(install)

class InstallTests(unittest.TestCase):
    def test_migrate_preserve_choices_and_restore_exact_config(self):
        with tempfile.TemporaryDirectory() as tmp:
            card=Path(tmp)
            original=json.dumps({'resPath':'/res/ui/','rotateScreen':270,
                'startupLibPath':'/mnt/extsd/lib/libadb-enabler.so'}).encode()
            (card/'EasyUI.cfg').write_bytes(original)
            (card/'adb-enabler').mkdir();(card/'adb-enabler/wifi.conf').write_text('Test network\nexample-passphrase\n')
            install.stage(card,configure=True)
            self.assertEqual((card/install.BACKUP).read_bytes(),original)
            self.assertEqual(json.loads((card/'EasyUI.cfg').read_bytes())['startupLibPath'],install.STARTUP)
            self.assertEqual((card/'Dashborne/wifi.conf').read_bytes(),(card/'adb-enabler/wifi.conf').read_bytes())
            self.assertTrue((card/'Dashborne/settings.conf').read_text().startswith('wifi_adb=1\n'))
            settings=b'wifi_adb=0\npatch.dashborne-badge=0\n'
            (card/'Dashborne/settings.conf').write_bytes(settings)
            install.stage(card)
            self.assertEqual((card/'Dashborne/settings.conf').read_bytes(),settings)
            self.assertEqual((card/install.BACKUP).read_bytes(),original)
            install.stage(card,restore=True)
            self.assertEqual((card/'EasyUI.cfg').read_bytes(),original)
