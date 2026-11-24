import importlib.util
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('config_host',ROOT/'configuration.py')
host=importlib.util.module_from_spec(spec);spec.loader.exec_module(host)

class ConfigTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp=tempfile.TemporaryDirectory(prefix='dashborne-config-')
        cls.root=Path(cls.tmp.name);cls.card=cls.root/'card';cls.bin=cls.root/'test'
        subprocess.run([os.environ.get('HOST_CC','cc'),'-std=c11','-Wall','-Wextra','-Werror',
                        '-fsanitize=address,undefined','-g',f'-DCONFIG_CARD="{cls.card}"',
                        str(ROOT/'config.c'),str(ROOT/'tests/config_test.c'),'-o',str(cls.bin)],check=True)
    @classmethod
    def tearDownClass(cls):cls.tmp.cleanup()
    def setUp(self):
        shutil.rmtree(self.card,ignore_errors=True);(self.card/'Dashborne').mkdir(parents=True)
        self.file=self.card/'dashborne.conf'
    def call(self,*args,success=True):
        p=subprocess.run([str(self.bin),*args],capture_output=True)
        self.assertEqual(p.returncode,0 if success else 1,p.stderr.decode())
        return p.stdout.decode().removesuffix('\n')
    def test_fresh_configuration_has_no_implicit_tweaks(self):
        expected=host.parse(host.migrate(self.card))
        self.call('load')
        actual=host.parse(self.file.read_bytes())
        self.assertEqual(actual,expected)
        self.assertFalse(any(key.startswith('tweak.') for key in actual))

    def test_migration_and_authoritative_root(self):
        (self.card/'Dashborne/settings.conf').write_text('wifi_adb=1\npatch.test=1\npatch.missing=0\n')
        (self.card/'Dashborne/wifi.conf').write_text(' Network #= \\ "\n Password #= \\ "\n')
        expected=host.parse(host.migrate(self.card))
        self.call('load');self.assertEqual(host.parse(self.file.read_bytes()),expected)
        self.assertFalse((self.card/'Dashborne/settings.conf').exists())
        self.assertFalse((self.card/'Dashborne/wifi.conf').exists())
        self.call('set','adb.enabled','0')
        (self.card/'Dashborne/settings.conf').write_text('wifi_adb=1\n')
        self.assertEqual(self.call('get','adb.enabled'),'0')
        self.assertEqual(self.call('wifi'),' Network #= \\ "\n Password #= \\ "')
    def test_comments_unknowns_and_literal_values_survive_updates(self):
        self.file.write_bytes(b'# Future preferences\r\nformat=1\r\nfuture.color=#aabbcc\r\nadb.enabled=0\r\nfuture.note=two = words')
        before=self.file.read_bytes()
        self.call('set','adb.enabled','1')
        self.assertEqual(self.file.read_bytes(),host.update(before,{'adb.enabled':'1'}))
        self.call('set','loader.app','/mnt/extsd/My apps/Sample.so')
        self.assertEqual(self.call('get','loader.app'),'/mnt/extsd/My apps/Sample.so')
        self.assertIn(b'future.note=two = words\n',self.file.read_bytes())
    def test_bad_documents_are_not_overwritten_or_migrated(self):
        (self.card/'Dashborne/settings.conf').write_text('wifi_adb=1\n')
        for raw in [b'',b'format=2\n',b'format=1\nadb.enabled=1\nadb.enabled=0\n',
                    b'format=1\nbad line\n',b'format=1\nx=bad\0data\n',b'format=1\nx=bad\rdata\n',
                    b'format=1\nx='+b'x'*1024,b'format=1\n#'+b'x'*32768]:
            self.file.write_bytes(raw)
            self.call('load',success=False);self.call('set','adb.enabled','1',success=False)
            self.assertEqual(self.file.read_bytes(),raw)
            with self.assertRaises(ValueError):host.parse(raw)
    def test_wifi_boundaries_and_failure(self):
        for ssid,password,valid in [('s'*32,'p'*63,True),('','password',False),('s'*33,'password',False),('ssid','short',False),('ssid','p'*64,False),(' háló #= ','password #=',True)]:
            self.file.write_bytes(host.update(b'format=1\n',{'wifi.ssid':ssid,'wifi.password':password}))
            output=self.call('wifi',success=valid)
            if valid:self.assertEqual(output,ssid+'\n'+password)
    def test_failed_atomic_save_keeps_previous_file(self):
        self.file.write_bytes(b'format=1\nadb.enabled=0\n')
        (self.card/'dashborne.conf.next').mkdir()
        self.call('set','adb.enabled','1',success=False)
        self.assertEqual(self.file.read_bytes(),b'format=1\nadb.enabled=0\n')
    def test_migration_failure_keeps_legacy_files(self):
        legacy=self.card/'Dashborne/wifi.conf';legacy.write_bytes(b'bad config')
        self.call('load',success=False);self.assertFalse(self.file.exists());self.assertEqual(legacy.read_bytes(),b'bad config')
        legacy.write_text('TestNetwork\npassword123\n');(self.card/'dashborne.conf.next').mkdir()
        self.call('load',success=False);self.assertTrue(legacy.exists());self.assertFalse(self.file.exists())
