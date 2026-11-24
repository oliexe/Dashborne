"""Run the real supervisor against disposable processes and a fake proc tree."""
from pathlib import Path
import signal
import subprocess
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[1]


class WatchdogTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='db-watch-', dir='/tmp')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.state = self.root/'state'; self.state.mkdir()
        self.card = self.root/'card'; self.card.mkdir()
        self.proc = self.root/'proc'; self.proc.mkdir()
        self.exe = self.root/'watchdog'
        shim = self.root/'host.c'
        shim.write_text("""#undef open
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>
int watchdog_reboot(void) {
    int fd=open(WD_ROOT "/reboot-requested",O_WRONLY|O_CREAT,0600);
    if(fd>=0)close(fd);
    return -1; /* Exercise the UI-only fallback without rebooting the host. */
}
int wd_open(const char *path,int flags,...) {
    int mode=0;
    if(flags&0100){va_list ap;va_start(ap,flags);mode=va_arg(ap,int);va_end(ap);}
    int native=flags&3;
    if(flags&0100)native|=O_CREAT;
    if(flags&01000)native|=O_TRUNC;
    if(flags&04000)native|=O_NONBLOCK;
    return open(path,native,mode);
}
""")
        subprocess.run(['cc','-Dopen=wd_open', '-std=c11', '-Wall', '-Wextra', '-Werror',
                        '-fsanitize=address,undefined', '-DWD_TIMEOUT=2',
                        f'-DWD_ROOT="{self.state}"', f'-DWD_CARD="{self.card}"',
                        f'-DWD_PROC="{self.proc}"', str(ROOT/'watchdog_main.c'), str(shim),
                        '-o', str(self.exe)], check=True)
        self.target = subprocess.Popen(['sleep', '60'])
        self.addCleanup(self.stop, self.target)
        self.pid = str(self.target.pid)
        self.entry = self.proc/self.pid; self.entry.mkdir()
        self.set_identity('12345')
        (self.state/'armed').write_text(self.pid+'\n')
        self.beat(1)

    @staticmethod
    def stop(process):
        if process.poll() is None:
            process.kill()
        process.wait(timeout=3)

    def set_identity(self, token):
        (self.entry/'stat').write_text(f'{self.pid} (test worker) S '+'0 '*18+token+' 0\n')

    def beat(self, counter):
        temporary = self.state/'heartbeat.next'
        temporary.write_text(f'{self.pid} {counter}\n')
        temporary.replace(self.state/'heartbeat')

    def start(self):
        process = subprocess.Popen([str(self.exe), self.pid])
        self.addCleanup(self.stop, process)
        return process

    def wait_ready(self):
        deadline = time.monotonic()+2
        while not (self.state/'ready').exists():
            self.assertLess(time.monotonic(), deadline)
            time.sleep(.02)

    def test_healthy_heartbeat_and_explicit_disarm(self):
        supervisor = self.start(); self.wait_ready()
        for i in range(8):
            self.beat(i+2); time.sleep(.4)
            self.assertIsNone(supervisor.poll())
        (self.state/'armed').unlink()
        self.assertEqual(supervisor.wait(timeout=3), 0)
        self.assertIsNone(self.target.poll())
        self.assertFalse((self.card/'config.once').exists())
        self.assertFalse((self.state/'reboot-requested').exists())

    def test_halt_captures_before_recovery(self):
        previous = self.card/'diagnostics/watchdog-1'
        previous.mkdir(parents=True)
        (previous/'logcat.txt').write_text('stale capture')
        supervisor = self.start(); self.wait_ready()
        self.assertEqual(supervisor.wait(timeout=8), 0)
        self.assertEqual(self.target.wait(timeout=2), -signal.SIGKILL)
        self.assertTrue((self.card/'config.once').exists())
        self.assertTrue((self.state/'reboot-requested').exists())
        capture = self.card/'diagnostics/watchdog-1'
        self.assertEqual((capture/'reason.txt').read_text(), 'UI heartbeat stopped\n')
        self.assertIn('12345', (capture/'stat').read_text())
        self.assertFalse((capture/'logcat.txt').exists())  # Host has no /bin/logcat.
        self.assertFalse((self.state/'armed').exists())

    def test_exited_process_and_pid_reuse_are_not_signalled(self):
        supervisor = self.start(); self.wait_ready()
        self.set_identity('67890')
        self.assertEqual(supervisor.wait(timeout=8), 0)
        self.assertIsNone(self.target.poll())
        self.assertEqual((self.card/'diagnostics/watchdog-1/reason.txt').read_text(), 'UI process exited\n')

    def test_failed_boot_marker_never_reboots_into_a_crash_loop(self):
        (self.card/'config.once').mkdir()  # Simulate an unwritable marker.
        supervisor = self.start(); self.wait_ready()
        self.assertEqual(supervisor.wait(timeout=8), 0)
        self.assertEqual(self.target.wait(timeout=2), -signal.SIGKILL)
        self.assertFalse((self.state/'reboot-requested').exists())
        self.assertTrue((self.state/'recovery').exists())

    def test_budget_and_duplicate_supervisor(self):
        (self.state/'recoveries').write_text('3')
        self.assertEqual(self.start().wait(timeout=2), 4)
        self.assertFalse((self.state/'ready').exists())
        (self.state/'recoveries').write_text('0')
        first = self.start(); self.wait_ready()
        self.assertEqual(self.start().wait(timeout=2), 3)
        (self.state/'armed').unlink()
        self.assertEqual(first.wait(timeout=3), 0)
        self.assertIsNone(self.target.poll())

    def test_other_owner_disarms_without_removing_new_session(self):
        supervisor = self.start(); self.wait_ready()
        (self.state/'armed').write_text('999999\n')
        self.assertEqual(supervisor.wait(timeout=3), 0)
        self.assertEqual((self.state/'armed').read_text(), '999999\n')
        self.assertIsNone(self.target.poll())


if __name__ == '__main__':
    unittest.main()
