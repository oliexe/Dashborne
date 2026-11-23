"""Render original menu artwork. Run with uv run --with resvg-py==0.5.0."""
from pathlib import Path
import resvg_py

OUT = Path(__file__).resolve().parent / 'assets'
STAR = 'M24 8 28.9 18 40 19.6 32 27.4 33.9 38.4 24 33.2 14.1 38.4 16 27.4 8 19.6 19.1 18Z'


def star(color):
    return f'<path d="{STAR}" fill="none" stroke="{color}" stroke-width="2.5" stroke-linejoin="round"/>'


def render(name, width, height, body):
    svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}">'
           + body + '</svg>')
    (OUT / name).write_bytes(resvg_py.svg_to_bytes(svg_string=svg))


if __name__ == '__main__':
    OUT.mkdir(exist_ok=True)
    render('star.png', 48, 48, star('#ffffff'))
    render('star-pressed.png', 48, 48, star('#0099ff'))
    render('tweaks.png', 38, 186,
           '<g transform="translate(-5 4)">' + star('#ffffff') + '</g>'
           '<text transform="translate(29 184) rotate(-90)" font-family="Arial" '
           'font-size="34" fill="#ffffff">Tweaks</text>')
