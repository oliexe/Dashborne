"""Inspect DASH input devices and collect a bounded evdev trace over ADB."""
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import re
import secrets
import shlex
import subprocess
import struct

INSPECTIONS = {
    'system': 'uname -a',
    'devices': 'cat /proc/bus/input/devices',
    'nodes': 'ls -l /dev/input',
    'processes': 'ps',
    'sockets': 'cat /proc/net/unix',
    'tools': 'for tool in getevent timeout nohup cat; do command -v "$tool" || :; done',
}


def adb(serial, *args, check=True):
    result = subprocess.run(['adb', '-s', serial, *args], text=True,
                            capture_output=True, timeout=20)
    if check and result.returncode:
        raise RuntimeError(result.stderr.strip() or result.stdout.strip() or 'ADB command failed')
    return result


def connected(serial):
    if adb(serial, 'get-state').stdout.strip() != 'device':
        raise RuntimeError('Enable ADB and connect to the DASH before capturing input.')


def inspect(serial, output):
    connected(serial)
    output.mkdir(parents=True, exist_ok=True)
    result = {'serial': serial, 'at': datetime.now(timezone.utc).isoformat(), 'commands': {}}
    for name, command in INSPECTIONS.items():
        run = adb(serial, 'shell', command, check=False)
        result['commands'][name] = {'command': command, 'exit': run.returncode,
                                    'stdout': run.stdout, 'stderr': run.stderr}
    (output / 'inventory.json').write_text(json.dumps(result, indent=2) + '\n')


def remote_path(value):
    if not re.fullmatch(r'/mnt/extsd/Dashborne/input-capture-[a-f0-9]{12}', value):
        raise ValueError('Invalid capture directory in session file')
    return value


def start_script(remote, seconds):
    remote = remote_path(remote)
    if not 5 <= seconds <= 180:
        raise ValueError('Capture duration must be between 5 and 180 seconds')
    # timeout runs on the DASH; losing ADB when the stock UI changes Wi-Fi must
    # neither stop the trace nor leave an unbounded observer running.
    runner = f'''cat /proc/uptime > {remote}/started.txt
if command -v getevent >/dev/null; then
  echo getevent > {remote}/format.txt
  timeout {seconds} getevent -lt > {remote}/events.txt 2>&1
  rc=$?
else
  echo raw-evdev > {remote}/format.txt
  count=0
  for device in /dev/input/event*; do
    [ -c "$device" ] || continue
    name=${{device##*/}}
    number=${{name#event}}
    case "$number" in ''|*[!0-9]*) continue;; esac
    count=$((count+1))
    (timeout {seconds} cat "$device" > {remote}/"$name".bin 2> {remote}/"$name".error
     printf "%s\\n" "$?" > {remote}/"$name".exit) &
  done
  wait
  rc=0
  [ "$count" -gt 0 ] || rc=2
fi
printf "%s\\n" "$rc" > {remote}/exit.txt
cat /proc/uptime > {remote}/finished.txt'''
    return ("set -eu\n"
            "for tool in timeout nohup cat; do\n"
            "  command -v \"$tool\" >/dev/null || { echo \"Missing $tool\" >&2; exit 1; }\n"
            "done\n"
            "timeout 1 sh -c 'exit 0'\n"
            f'mkdir {shlex.quote(remote)}\n'
            f'nohup sh -c {shlex.quote(runner)} > {remote}/runner.txt 2>&1 < /dev/null &\n'
            'printf "%s\\n" "$!"\n')


def start(serial, output, seconds):
    if (output / 'session.json').exists():
        raise RuntimeError('This output directory already contains a session; choose a new directory.')
    inspect(serial, output)
    remote = '/mnt/extsd/Dashborne/input-capture-' + secrets.token_hex(6)
    # Save the destination before launch so a transport loss cannot orphan the
    # capture location. "requested" deliberately does not mean it is running.
    session = {'serial': serial, 'remote': remote, 'seconds': seconds, 'state': 'requested'}
    path = output / 'session.json'
    path.write_text(json.dumps(session, indent=2) + '\n')
    run = adb(serial, 'shell', start_script(remote, seconds))
    pid = run.stdout.strip()
    if not pid.isdecimal():
        raise RuntimeError('Capture did not return a process ID; inspect the saved session.')
    session.update(state='launched', pid=int(pid))
    path.write_text(json.dumps(session, indent=2) + '\n')
    print(f'Capture launched for {seconds} seconds. Session: {path}')
    print('Launch the stock dashboard and perform the input sequence. Reconnect ADB afterward and run pull.')


def pull(output):
    session = json.loads((output / 'session.json').read_text())
    remote = remote_path(session['remote'])
    connected(session['serial'])
    destination = output / 'device'
    destination.mkdir(exist_ok=True)
    adb(session['serial'], 'pull', remote + '/.', str(destination))
    status = destination / 'exit.txt'
    if not status.exists():
        print(f'Partial capture saved to {destination}; completion has not been confirmed.')
    else:
        print(f'Capture saved to {destination}; collector exit status: {status.read_text().strip()}')
    inventory = json.loads((output / 'inventory.json').read_text())
    if 'armv7l' in inventory['commands']['system']['stdout'].split():
        for raw in destination.glob('event[0-9]*.bin'):
            try:
                rows = decode_arm_evdev(raw.read_bytes())
                raw.with_suffix('.jsonl').write_text(''.join(json.dumps(row) + '\n' for row in rows))
            except ValueError as error:
                print(f'{raw.name}: {error}; raw capture retained.')


def decode_arm_evdev(data):
    # Linux ARMv7's 32-bit reader ABI: timeval (two longs), type/code, value.
    # Only used after inventory identifies armv7l; other platforms keep raw data.
    if len(data) % 16:
        raise ValueError('incomplete 16-byte input record')
    rows = []
    for seconds, usec, kind, code, value in struct.iter_unpack('<llHHi', data):
        if not 0 <= usec < 1000000 or kind > 0x1f:
            raise ValueError('unexpected input layout')
        rows.append(dict(seconds=seconds, microseconds=usec, type=kind, code=code, value=value))
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('inspect', 'start', 'pull'))
    parser.add_argument('--serial', default='10.0.1.35:5555')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--seconds', type=int, default=90)
    args = parser.parse_args()
    try:
        if args.action == 'inspect': inspect(args.serial, args.output)
        elif args.action == 'start': start(args.serial, args.output, args.seconds)
        else: pull(args.output)
    except (RuntimeError, ValueError, OSError, subprocess.TimeoutExpired) as error:
        parser.exit(1, f'{error}\n')


if __name__ == '__main__':
    main()
