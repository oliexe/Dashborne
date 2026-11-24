"""Render the core size previews and Widget Example's picker artwork.

Run: uv run --with resvg-py==0.5.0 python tools/widget_previews.py
"""
from pathlib import Path
import resvg_py

ROOT = Path(__file__).resolve().parents[1]
SIZES = [(1, 1, 250, 224), (2, 1, 510, 224), (1, 2, 250, 458),
         (2, 2, 510, 458), (3, 2, 770, 458), (4, 2, 1030, 458),
         (5, 2, 1290, 458), (6, 2, 1550, 458)]


def artwork(width, height, example):
    scale = min(480 / width, 312 / height, 1)
    out_w, out_h = round(width * scale), round(height * scale)
    body = [f'<rect x="1" y="1" width="{width-2}" height="{height-2}" '
            'rx="18" fill="#0a0a0a" stroke="#5a5d60" stroke-width="2"/>']
    if example:
        font = 16 if width == 250 else 22
        action = 64 if width == 250 else 108
        body += [f'<text x="16" y="37" fill="#999999" font-size="{font}">Widget Example</text>',
                 f'<rect x="{width-action-16}" y="12" width="{action}" height="36" '
                 'fill="#22282c" rx="3"/>',
                 f'<text x="{width-action/2-16}" y="37" text-anchor="middle" '
                 'fill="#e1e1e1" font-size="18">Tap</text>',
                 f'<text x="{width/2}" y="{height/2+16}" text-anchor="middle" '
                 'fill="#e1e1e1" font-size="46">0</text>',
                 f'<text x="{width/2}" y="{height-24}" text-anchor="middle" '
                 f'fill="#999999" font-size="{16 if width==250 else 19}">{width} × {height} px</text>']
    else:
        body += [f'<rect x="18" y="24" width="{min(width-36,140)}" height="12" rx="6" fill="#656a70"/>',
                 f'<rect x="{width/2-35}" y="{height/2-23}" width="70" height="32" rx="6" fill="#d5d9df"/>',
                 f'<rect x="{width/2-46}" y="{height-36}" width="92" height="10" rx="5" fill="#656a70"/>']
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{out_w}" height="{out_h}" '
            f'viewBox="0 0 {width} {height}" font-family="Arial">' + ''.join(body) + '</svg>')


if __name__ == '__main__':
    for example, folder in [(False, 'dashborne/core/widget-runtime/assets'),
                            (True, 'extensions/widget-example/assets')]:
        output = ROOT / folder
        output.mkdir(parents=True, exist_ok=True)
        for cols, rows, width, height in SIZES:
            (output / f'preview-{cols}x{rows}.png').write_bytes(
                resvg_py.svg_to_bytes(svg_string=artwork(width, height, example)))
