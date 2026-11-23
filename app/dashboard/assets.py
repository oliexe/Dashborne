"""Generate original vector artwork and lightweight PNGs for the cluster."""
import json
import math
from pathlib import Path
import resvg_py

ROOT = Path(__file__).resolve().parent
OUT = ROOT / 'card/ui/cluster'
BUILD = ROOT / 'build'
CAR = (ROOT / 'assets/car.svg').read_text().split('>', 1)[1].rsplit('</svg>', 1)[0]
BG = '#050607'


def svg(width, height, content):
    return f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">{content}</svg>'


def render(name, width, height, content):
    source = svg(width, height, content)
    (OUT / name).write_bytes(resvg_py.svg_to_bytes(svg_string=source, skip_system_fonts=True))
    (BUILD / (name.removesuffix('.png') + '.svg')).write_text(source)
    return source


def road(phase, parked=False):
    items = [f'<rect width="680" height="380" fill="{BG}"/>', '''
      <defs>
        <linearGradient id="road" x1="0" y1="0" x2="0" y2="1"><stop stop-color="#050607"/><stop offset="1" stop-color="#12161c"/></linearGradient>
        <linearGradient id="lane" x1="0" y1="0" x2="0" y2="1"><stop stop-color="#29597f" stop-opacity="0"/><stop offset=".45" stop-color="#398eff"/><stop offset="1" stop-color="#4099ff"/></linearGradient>
      </defs>''']
    if not parked:
        items.append('<path d="M320 52 L362 52 552 368 128 368Z" fill="url(#road)"/>')
        for side in (-1, 1):
            for i in range(5):
                depth = ((i + phase / 8) / 5) % 1
                end = min(depth + .065 + depth * .035, 1)
                y0, y1 = 50 + depth**1.5 * 320, 50 + end**1.5 * 320
                x0, x1 = 341 + side * (36 + depth**1.5 * 285), 341 + side * (36 + end**1.5 * 285)
                items.append(f'<path d="M{x0:.1f} {y0:.1f}L{x1:.1f} {y1:.1f}" stroke="#59626d" stroke-opacity="{.18 + depth*.35:.2f}" stroke-width="{1+depth*3:.1f}"/>')
        for x in (154, 526):
            top = 328 if x < 340 else 354
            items.append(f'<path d="M{top} 56Q{top} 130 {x} 368" fill="none" stroke="#236cb7" stroke-opacity=".14" stroke-width="13"/>')
            items.append(f'<path d="M{top} 56Q{top} 130 {x} 368" fill="none" stroke="url(#lane)" stroke-width="3.5"/>')
        items.append(f'<g transform="translate(317 98) scale(.24)" opacity=".5">{CAR}</g>')
        items.append(f'<g transform="translate(451 163) scale(.29)" opacity=".25">{CAR}</g>')
    else:
        items.append('<ellipse cx="340" cy="307" rx="160" ry="50" fill="#10141a"/>')
    items.append(f'<g transform="translate(263 161) scale(.77)">{CAR}</g>')
    return ''.join(items)


def nav(energy=False):
    out = [f'<rect width="560" height="380" fill="{BG}"/>']
    if energy:
        for y in (218, 258, 298):
            out.append(f'<path d="M62 {y}H500" stroke="#1e252e"/>')
        points = [(62+i*11, 258 + math.sin(i*.45)*17 + math.sin(i*1.3)*12) for i in range(41)]
        line = 'M'+'L'.join(f'{x:.1f} {y:.1f}' for x,y in points)
        out.append('<defs><linearGradient id="fill" x1="0" y1="0" x2="0" y2="1"><stop stop-color="#4ac994" stop-opacity=".22"/><stop offset="1" stop-color="#4ac994" stop-opacity="0"/></linearGradient></defs>')
        out.append(f'<path d="{line}L502 327H62Z" fill="url(#fill)"/>')
        out.append(f'<path d="{line}" fill="none" stroke="#54cd9c" stroke-width="2.4"/>')
        out.append('<path d="M66 106H103L84 139H112L70 184 82 148H60Z" fill="#54cd9c"/>')
    else:
        roads = ['M10 232L160 280 250 260 390 294 530 260','M-10 308L160 348 292 316 440 353 565 310','M22 376L166 402 296 367 440 402','M84 190L158 280 167 402','M245 184L250 260 292 316 296 380','M390 214L390 294 440 353 440 400','M530 185L530 260 565 310']
        for path in roads:
            out.append(f'<path d="{path}" fill="none" stroke="#171f28" stroke-width="6"/>')
            out.append(f'<path d="{path}" fill="none" stroke="#242c37" stroke-width="1"/>')
        out.append('<path d="M178 360L238 330Q252 322 262 321L339 302Q355 297 354 278L352 247" fill="none" stroke="#123e70" stroke-width="11"/>')
        out.append('<path d="M178 360L238 330Q252 322 262 321L339 302Q355 297 354 278L352 247" fill="none" stroke="#4799f8" stroke-width="4"/>')
        out.append('<circle cx="185" cy="356" r="18" fill="#0e213c" stroke="#28629b"/><path d="M174 365L190 342 195 369 185 362Z" fill="#e8f2ff"/>')
        out.append('<path d="M77 172V137Q77 117 97 117H116M103 104L117 117 103 130" fill="none" stroke="#f0f3f7" stroke-width="7" stroke-linecap="round" stroke-linejoin="round"/>')
    return ''.join(out)


def main():
    OUT.mkdir(parents=True, exist_ok=True); BUILD.mkdir(exist_ok=True)
    base = f'<rect width="1920" height="480" fill="{BG}"/>'
    base += '''<rect x="61" y="431" width="38" height="17" rx="3" fill="none" stroke="#7c8996" stroke-width="1.5"/>
      <rect x="65" y="435" width="25" height="9" rx="1" fill="#52c591"/><rect x="101" y="436" width="3" height="7" rx="1" fill="#7c8996"/>
      <circle cx="1765" cy="139" r="32" fill="#f4f4f4" stroke="#d95454" stroke-width="5"/>
      <circle cx="1765" cy="285" r="18" fill="#172f4b" stroke="#499afa" stroke-width="2"/>
      <circle cx="1765" cy="285" r="11" fill="none" stroke="#75b4ff" stroke-width="2"/>
      <path d="M1754 282L1761 286 1765 296 1769 286 1776 282M1761 286H1769" fill="none" stroke="#75b4ff" stroke-width="2"/>
      <rect x="1857" y="100" width="3" height="240" rx="1.5" fill="#222831"/>
      <rect x="1857" y="205" width="3" height="48" rx="1.5" fill="#aebbc9"/>
      <path d="M583 105V360" stroke="#10141a"/>
      <circle cx="72" cy="43" r="3" fill="#4d9dfb"/>'''
    render('base.png',1920,480,base)
    render('base-park.png',1920,480,base + f'<rect x="1718" y="199" width="100" height="120" fill="{BG}"/>')
    render('nav.png',560,380,nav())
    render('energy.png',560,380,nav(True))
    for frame in range(8): render(f'road-{frame}.png',680,380,road(frame))
    render('park.png',680,380,road(0,True))
    # Local composition preview; production text is rendered by native controls.
    layout = json.loads((ROOT/'screen.json').read_text())
    preview = [base, f'<g transform="translate(0 30)">{nav()}</g>',f'<g transform="translate(680 20)">{road(0)}</g>']
    from html import escape
    for key, control in layout.items():
        if not key.startswith('textview__') or not control.get('text'): continue
        p=control['position']; size=control['fontSize']; center=control['alignment']==37
        x=p['left']+(p['width']/2 if center else 0); y=p['top']+p['height']/2+size*.34
        color=control['colorTab']['color0']
        preview.append(f'<text x="{x}" y="{y}" font-size="{size}" font-family="Helvetica Neue" text-anchor="{"middle" if center else "start"}" fill="#{color:06x}">{escape(control["text"])}</text>')
    (BUILD/'cluster-preview.png').write_bytes(resvg_py.svg_to_bytes(svg_string=svg(1920,480,''.join(preview))))
    print('Built original cluster artwork: 13 PNG assets')

if __name__ == '__main__': main()
