"""Inventory a fresh ADB UI capture and exercise every PNG through the adapter.

The report contains names, dimensions and coverage, not stock image contents.
"""
import argparse
import ctypes
import json
from pathlib import Path
import struct
import subprocess
import tempfile

from prepare import PAGES
from theme import ROOT
from ui_codec import decode


def audit(capture):
    authored = set(json.loads((ROOT / 'resource-sizes.json').read_text()))
    report = {'target_os': '6.11.4', 'layouts': [], 'images': [], 'visual_checks': []}
    with tempfile.TemporaryDirectory() as work:
        library = Path(work) / 'engine.so'
        subprocess.run(['clang', '-shared', '-fPIC', str(ROOT / 'theme_engine.c'), '-lz', '-o', str(library)], check=True)
        lib = ctypes.CDLL(str(library))
        lib.theme_png.argtypes = [ctypes.c_char_p, ctypes.c_uint, ctypes.c_int, ctypes.c_uint,
                                 ctypes.c_uint, ctypes.POINTER(ctypes.c_void_p), ctypes.POINTER(ctypes.c_uint)]
        free = ctypes.CDLL(None).free; free.argtypes = [ctypes.c_void_p]
        for path in sorted(capture.glob('*.ftu')):
            decode(path.read_bytes())
            report['layouts'].append({'name': path.name, 'themed': path.stem in PAGES})
        for path in sorted(capture.rglob('*.png')):
            name = path.relative_to(capture).as_posix()
            blob = path.read_bytes(); output, length = ctypes.c_void_p(), ctypes.c_uint()
            supported = bool(lib.theme_png(blob, len(blob), name.startswith('light/'), 0x17181a,
                                           0xf4f4f4, ctypes.byref(output), ctypes.byref(length)))
            free(output)
            report['images'].append({'name': name, 'size': list(struct.unpack_from('>II', blob, 16)),
                                     'coverage': 'authored' if name in authored else 'palette' if supported else 'uncovered',
                                     'decoder_verified': supported})
    report['summary'] = {kind: sum(r['coverage'] == kind for r in report['images'])
                         for kind in ('authored', 'palette', 'uncovered')}
    report['summary']['layouts'] = len(report['layouts'])
    return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    report = audit(args.capture)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report['summary']))
    raise SystemExit(bool(report['summary']['uncovered'] or any(not p['themed'] for p in report['layouts'])))
