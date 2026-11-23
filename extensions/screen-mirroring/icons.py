"""Render original line icons. Run with uv run --with resvg-py==0.5.0."""
from pathlib import Path
import resvg_py
OUT=Path(__file__).resolve().parent/'assets'
PATHS={
    'back':'<path d="M38 19 25 32 38 45 M26 32H47"/>',
    'aspect':'<path d="M25 19H19V25 M39 19H45V25 M19 39V45H25 M45 39V45H39"/><rect x="25" y="25" width="14" height="14" rx="1"/>',
    'retry':'<path d="M45 27A14 14 0 1 0 45 38 M45 17V27H35"/>',
}
def svg(name,pressed=False):
    bg='#383c42' if pressed else '#17191d'
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="64" height="64" viewBox="0 0 64 64">'
            f'<rect width="64" height="64" rx="16" fill="{bg}"/>'
            '<g fill="none" stroke="#f2f3f5" stroke-width="3" stroke-linecap="round" stroke-linejoin="round">'
            +PATHS[name]+'</g></svg>')
if __name__=='__main__':
    OUT.mkdir(exist_ok=True)
    for name in PATHS:
        for pressed in (False,True):
            (OUT/f'{name}{"-pressed" if pressed else ""}.png').write_bytes(
                resvg_py.svg_to_bytes(svg_string=svg(name,pressed),skip_system_fonts=True))
