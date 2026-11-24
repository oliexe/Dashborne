from pathlib import Path
import re
import struct

source = Path(__file__).resolve().parents[2] / 'tesla-ui'
rows = re.findall(r'\{"([^"]+)",(\d+),(\d+),(\d+),(\d+)\}',
                  (source / 'motion_data.h').read_text())
assert len(rows) == 347
sizes = []
for name, x, y, width, height in rows:
    raw = (source / 'assets/motion' / name).read_bytes()
    width, height = int(width), int(height)
    assert raw[:8] == b'\x89PNG\r\n\x1a\n'
    assert struct.unpack('>II', raw[16:24]) == (width, height)
    sizes.append(width * height * 4)
camera = sum(sizes[:65])
endpoint = max(sizes[65:193])
hinges = max(sum(sizes[start:start + 11]) for start in range(193, 347, 11))
peak = camera + endpoint + hinges
assert peak < 20 * 1024 * 1024, peak
print(f'Vehicle widget decoded-pixel budget: {peak / 1024 / 1024:.2f} MiB')
