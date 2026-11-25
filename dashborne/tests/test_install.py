import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('loader_install',ROOT/'install.py')
install=importlib.util.module_from_spec(spec);spec.loader.exec_module(install)

class InstallTests(unittest.TestCase):
    def test_retired_packages_removed_without_losing_user_data(self):
        with tempfile.TemporaryDirectory() as tmp:
            card=Path(tmp)
            install.stage(card)
            settings=b'format=1\nadb.enabled=0\nwidgets.source.530=my-widget\nvehicle-widget.layout.bytes=2\nvehicle-widget.layout.0=7b7d\n'
            (card/'dashborne.conf').write_bytes(settings)
            for old in install.RETIRED_PACKAGES:
                folder=card/old;folder.mkdir(parents=True)
                (folder/'libextension.so').write_bytes(b'old core')
            for old in install.RETIRED_FILES:
                (card/old).write_bytes(b'old patch')
            user=card/'Dashborne/extensions/my-widget';user.mkdir()
            (user/'libextension.so').write_bytes(b'user widget')
            install.stage(card)
            self.assertEqual((card/'dashborne.conf').read_bytes(),settings)
            self.assertEqual((user/'libextension.so').read_bytes(),b'user widget')
            self.assertTrue(all(not (card/old).exists() for old in install.RETIRED_PACKAGES))
            self.assertTrue(all(not (card/old).exists() for old in install.RETIRED_FILES))

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
            values=install.configuration.parse((card/'dashborne.conf').read_bytes())
            self.assertEqual(values['wifi.ssid'],'Test network')
            self.assertEqual(values['adb.enabled'],'0')
            self.assertFalse((card/'Dashborne/wifi.conf').exists())
            settings=b'# Keep this comment\nformat=1\nadb.enabled=1\ntweak.dashborne-badge.enabled=0\nfuture.option=custom\n'
            (card/'dashborne.conf').write_bytes(settings)
            install.stage(card)
            self.assertEqual((card/'dashborne.conf').read_bytes(),settings)
            self.assertEqual((card/install.BACKUP).read_bytes(),original)
            install.stage(card,restore=True)
            self.assertEqual((card/'EasyUI.cfg').read_bytes(),original)

    def test_existing_dashborne_migration_and_invalid_config(self):
        with tempfile.TemporaryDirectory() as tmp:
            card=Path(tmp);(card/'Dashborne').mkdir()
            original=json.dumps({'resPath':'/res/ui/','startupLibPath':install.STARTUP}).encode()
            (card/'EasyUI.cfg').write_bytes(original);(card/install.BACKUP).write_bytes(original)
            (card/'Dashborne/settings.conf').write_text('wifi_adb=1\npatch.contrast=1\n')
            (card/'Dashborne/wifi.conf').write_text('TestNetwork\npassword123\n')
            install.stage(card)
            values=install.configuration.parse((card/'dashborne.conf').read_bytes())
            self.assertEqual(values['adb.enabled'],'1');self.assertEqual(values['tweak.contrast.enabled'],'1')
            self.assertFalse((card/'Dashborne/settings.conf').exists())
            (card/'dashborne.conf').write_text('format=999\n')
            before=(card/'EasyUI.cfg').read_bytes()
            with self.assertRaises(ValueError):install.stage(card)
            self.assertEqual((card/'dashborne.conf').read_text(),'format=999\n')
            self.assertEqual((card/'EasyUI.cfg').read_bytes(),before)

    def test_fresh_card_install_update_and_restore(self):
        with tempfile.TemporaryDirectory() as tmp:
            card=Path(tmp)
            install.stage(card)
            active=json.loads((card/'EasyUI.cfg').read_bytes())
            expected=dict(install.DEFAULT_STARTUP, startupLibPath=install.STARTUP)
            self.assertEqual(active,expected)
            self.assertEqual(json.loads((card/install.BACKUP).read_bytes()),install.DEFAULT_STARTUP)
            self.assertTrue((card/'Dashborne/ui/loader.ftu').is_file())
            self.assertTrue((card/'Dashborne/assets/extra-menu/menu.ftu').is_file())
            self.assertFalse((card/'Dashborne/extensions/extra-menu').exists())
            self.assertFalse((card/'Dashborne/extensions/widget-runtime').exists())
            self.assertEqual(install.configuration.parse((card/'dashborne.conf').read_bytes())['adb.enabled'],'0')
            original=(card/install.BACKUP).read_bytes()
            install.stage(card)
            self.assertEqual((card/install.BACKUP).read_bytes(),original)
            install.stage(card,restore=True)
            self.assertEqual((card/'EasyUI.cfg').read_bytes(),original)

    def test_missing_build_leaves_fresh_card_empty(self):
        with tempfile.TemporaryDirectory() as tmp:
            card=Path(tmp)
            with self.assertRaises(OSError):
                install.stage(card,build=card/'missing-build')
            self.assertEqual(list(card.iterdir()),[])
