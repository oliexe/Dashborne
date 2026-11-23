import json
from pathlib import Path
import subprocess
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from deploy import ACTIVATE, START, RESTORE, configuration, LIBRARY


class DeploymentTests(unittest.TestCase):
    def test_preserves_unrelated_config_and_selects_injector(self):
        original = {'startupLibPath': '/mnt/extsd/lib/libadb-enabler.so',
                    'resPath': '/res/ui/', 'rotateTouch': 0, 'rotateScreen': 270}
        changed = json.loads(configuration(json.dumps(original)))
        self.assertEqual(changed.pop('startupLibPath'), LIBRARY)
        original.pop('startupLibPath')
        self.assertEqual(changed, original)

    def test_rejects_wrong_resource_root_and_duplicate_install(self):
        for config in ({'resPath': '/mnt/extsd/ui/'},
                       {'resPath': '/res/ui/', 'startupLibPath': LIBRARY}):
            with self.assertRaises(ValueError):
                configuration(json.dumps(config))

    def test_scripts_parse_and_restore_saves_config_before_reboot(self):
        for script in (START, RESTORE, ACTIVATE):
            subprocess.run(['sh', '-n'], input=script.encode(), check=True)
        self.assertLess(RESTORE.index('cp '), RESTORE.index('\nsync'))
        self.assertLess(RESTORE.index('\nsync'), RESTORE.index('\nreboot'))
        self.assertNotIn('ctl.start', RESTORE)
        self.assertNotIn('ping -c 121', ACTIVATE)


if __name__ == '__main__':
    unittest.main()
