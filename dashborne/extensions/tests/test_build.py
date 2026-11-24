import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('extension_builder', Path(__file__).resolve().parents[1] / 'build.py')
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)


class SharedAssetsTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.sources = self.root / 'extensions'
        self.consumer = self.sources / 'consumer'
        self.consumer.mkdir(parents=True)
        self.assets = self.sources / 'provider/assets'
        (self.assets / 'motion').mkdir(parents=True)
        (self.assets / 'motion/frame.png').write_bytes(b'fixture')
        self.output = self.root / 'output'
        self.manifest = dict(id='consumer', title='Consumer', description='Fixture', abi=1,
                             asset_sources=[dict(extension='provider', paths=['motion'])])

    def build(self):
        (self.consumer / 'manifest.json').write_text(json.dumps(self.manifest))
        # Exercise packaging and path checks without invoking a cross-compiler.
        with patch.object(builder.subprocess, 'run'):
            builder.build(self.sources, self.output)

    def test_shared_files_are_copied_into_independent_extension(self):
        self.build()
        asset = self.output / 'consumer/assets/motion/frame.png'
        self.assertEqual(asset.read_bytes(), b'fixture')
        self.assertFalse(asset.is_symlink())

    def test_core_services_cannot_be_built_as_plugins(self):
        for id in ('extra-menu', 'widget-runtime'):
            self.manifest['id'] = id
            with self.assertRaisesRegex(ValueError, 'built into Dashborne'): self.build()
        self.manifest['id'] = 'consumer'
        self.manifest['core'] = True
        with self.assertRaisesRegex(ValueError, 'built into Dashborne'): self.build()

    def test_missing_and_escaping_paths_are_rejected(self):
        for name in ('missing', '../manifest.json', '/tmp', 'motion/../../outside'):
            with self.subTest(name=name):
                self.manifest['asset_sources'][0]['paths'] = [name]
                with self.assertRaises(ValueError): self.build()

    def test_symlink_escape_and_duplicate_destination_are_rejected(self):
        outside = self.root / 'outside'
        outside.write_bytes(b'outside')
        link = self.assets / 'motion/escape'
        link.symlink_to(outside)
        with self.assertRaises(ValueError): self.build()
        link.unlink()
        local = self.consumer / 'assets/motion'
        local.mkdir(parents=True)
        (local / 'frame.png').write_bytes(b'local')
        with self.assertRaisesRegex(ValueError, 'Duplicate'): self.build()
