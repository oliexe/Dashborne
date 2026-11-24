"""Prepare a local model pack from an extracted animation directory."""
import argparse
from pathlib import Path
import shutil
import struct
import tempfile

COLORS = [('gray', 'gray'), ('home', 'white'), ('black', 'black'), ('blue', 'blue'), ('red', 'red')]
SEQUENCES = {'P_N': (25, 482, 322), 'n_road': (25, 149, 361),
             'door_left_front': (16, 187, 162), 'door_left_back': (16, 165, 142),
             'door_right_front': (16, 182, 160), 'door_right_back': (16, 152, 139),
             'front_hood': (16, 220, 136), 'back_hood': (16, 184, 134)}
SHARED = {'dipped_light': (327, 208), 'distance_light': (327, 208),
          'back_left_light': (34, 19), 'back_right_light': (34, 19),
          'night_forward_gears_remind': (82, 190), 'night_back_gears_remind': (82, 190),
          'forward_gears_rectangle': (330, 428), 'back_gears_rectangle': (330, 428)}


def png(path, dimensions):
    data = path.read_bytes()
    allowed = [dimensions]
    if dimensions == (149, 361) and path.stem != 'n_road0':
        allowed.append((148, 345))
    if (len(data) < 33 or data[:8] != b'\x89PNG\r\n\x1a\n'
            or data[12:16] != b'IHDR' or struct.unpack('>II', data[16:24]) not in allowed):
        raise ValueError(f'Unexpected model image dimensions: {path}')
    return path


def prepare(source, destination, model='highland'):
    import re
    source, destination = Path(source).resolve(), Path(destination).resolve()
    if not re.fullmatch(r'[a-z0-9]+(?:-[a-z0-9]+)*', model) or len(model) > 32:
        raise ValueError('Use a short lowercase model ID')
    if destination.exists():
        raise ValueError('Destination already exists; use a new pack directory')
    # Validate all variants before writing anything. Copy original files unchanged.
    files = []
    for directory, color in COLORS:
        origin = source / directory
        if not origin.is_dir():
            continue
        variant = f'{model}-{color}'
        frames = [png(origin / 'naked_car.png', (482, 322))]
        for prefix, (count, width, height) in SEQUENCES.items():
            frames.extend(png(origin / f'{prefix}{i}.png', (width, height)) for i in range(count))
        frames.extend(png(source / 'home' / f'road{i}.png', (300, 300)) for i in range(20))
        frames.extend(png(source / 'home' / f'{name}.png', size) for name, size in SHARED.items())
        files.append((variant, frames))
    if not files:
        raise ValueError('No complete car animation directories found')
    destination.parent.mkdir(parents=True, exist_ok=True)
    staging = Path(tempfile.mkdtemp(prefix='.model-pack-', dir=destination.parent))
    try:
        for variant, frames in files:
            target = staging / variant
            target.mkdir()
            for frame in frames:
                shutil.copyfile(frame, target / frame.name)
        (staging / 'index').write_text('tesla-model-v1\n' + ''.join(f'{variant}\n' for variant, _ in files))
        staging.rename(destination)
    finally:
        if staging.exists():
            shutil.rmtree(staging)
    return [variant for variant, _ in files]


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path, help='Animation root containing gray, home, etc.')
    parser.add_argument('destination', type=Path, help='New models directory outside the source tree')
    parser.add_argument('--model', default='highland', help='Model ID prefix')
    args = parser.parse_args()
    print('Prepared: ' + ', '.join(prepare(args.source, args.destination, args.model)))
