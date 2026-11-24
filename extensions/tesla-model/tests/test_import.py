import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('import_model', Path(__file__).parents[1] / 'import_model.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class ImportTests(unittest.TestCase):
    def test_wrong_or_missing_frames_leave_no_pack(self):
        with tempfile.TemporaryDirectory() as root:
            root = Path(root)
            source = root / 'source'
            (source / 'gray').mkdir(parents=True)
            dest = root / 'models'
            with self.assertRaises(FileNotFoundError):
                module.prepare(source, dest)
            self.assertFalse(dest.exists())
            (source / 'gray/naked_car.png').write_bytes(b'not an image')
            with self.assertRaises(ValueError):
                module.prepare(source, dest)
            self.assertFalse(dest.exists())

    def test_model_path_cannot_escape(self):
        with tempfile.TemporaryDirectory() as root:
            for name in ('../escape', '/absolute', 'with space', 'x' * 33):
                with self.assertRaises(ValueError):
                    module.prepare(root, Path(root) / 'models', name)

    def test_expected_size_and_variant_size(self):
        with tempfile.TemporaryDirectory() as root:
            p = Path(root) / 'n_road1.png'
            p.write_bytes(b'\x89PNG\r\n\x1a\n' + struct.pack('>I', 13) + b'IHDR' + struct.pack('>II', 148, 345) + b'\0' * 9)
            self.assertEqual(module.png(p, (149, 361)), p)
            with self.assertRaises(ValueError):
                module.png(p, (482, 322))


if __name__ == '__main__':
    unittest.main()
