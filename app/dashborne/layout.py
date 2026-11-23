"""Original splash and configuration screen for the DASH's native UI runtime."""
from pathlib import Path
import sys
ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT.parent / 'dashboard'))
from layout import encode, decode


def position(x, y, w, h):
    return dict(left=x, top=y, width=w, height=h)


def label(id, text, x, y, w, h, size=24, color=0xdce2ee, align=36):
    return dict(id=id, text=text, fontSize=size, alignment=align,
                colorTab={'color0': color}, position=position(x, y, w, h), touchable=False)


def button(id, text, x, y, w, h, primary=False):
    p = label(id, text, x, y, w, h, color=0xffffff if primary else 0xdce2ee)
    p.update(touchable=True, backgroundColor=0x4e69d3 if primary else 0x151925,
             textPosition=position(20, 0, w-40, h))
    return p


def screen():
    root = dict(id=0, caption='Dashborne', backgroundColor=0x000000,
                position=position(0, 0, 1920, 480), resolution=dict(width=1920, height=480),
                visible=True, topmost=False, beepEnable=False, hideTimeout=-1)
    splash = dict(id=110001, position=position(0, 0, 1920, 480), visible=True)
    splash['button__1'] = dict(id=20001, position=position(0, 0, 1920, 480), touchable=True)
    splash['textview__2'] = dict(id=50010, position=position(840, 112, 240, 160), touchable=False,
                               backgroundPic='../../../mnt/extsd/Dashborne/ui/icon.png')
    splash['textview__3'] = label(50011, 'Tap screen to enter configuration', 560, 282, 800, 36, 22, 0xa4aabd, 37)
    config = dict(id=110002, position=position(0, 0, 1920, 480), visible=False, backgroundColor=0x090c14)
    config['textview__4'] = dict(id=50012, position=position(150, 22, 108, 72), touchable=False,
                               backgroundPic='../../../mnt/extsd/Dashborne/ui/icon.png')
    config['textview__5'] = label(50013, 'DASHBORNE', 280, 26, 700, 40, 30, 0xf0f2ff)
    config['textview__6'] = label(50014, 'Configure your dashboard', 280, 66, 900, 28, 19, 0x79849a)
    config['textview__7'] = label(50015, 'DEVELOPER ACCESS', 180, 132, 500, 30, 18, 0x79849a)
    config['button__8'] = button(20002, '[ ]  Wi-Fi ADB', 180, 181, 470, 64)
    config['textview__9'] = label(50002, 'Disabled', 180, 260, 500, 35, 22, 0x9ba6bd)
    config['textview__10'] = label(50003, '', 180, 302, 520, 44, 30, 0x8eb5ff)
    config['textview__11'] = label(50005, 'Patches on microSD', 770, 126, 800, 34, 22, 0x9ba6bd)
    for row in range(4):
        y = 170 + row * 52
        config[f'button__{20+row}'] = button(20100+row, '', 770, y, 970, 48)
        config[f'button__{20+row}']['textPosition']['height'] = 32
        config[f'textview__{30+row}'] = label(50100+row, '', 790, y+31, 930, 20, 15, 0x79849a)
    config['textview__40'] = label(50004, 'Settings are saved to microSD', 180, 411, 1000, 32, 19, 0x79849a)
    config['button__41'] = button(20004, 'Previous', 1120, 398, 140, 54)
    config['button__42'] = button(20005, 'Next', 1280, 398, 120, 54)
    config['button__43'] = button(20003, 'Launch dashboard', 1440, 398, 300, 54, primary=True)
    root['window__1'] = splash
    root['window__2'] = config
    return root


if __name__ == '__main__':
    out = ROOT / 'build/card/Dashborne/ui/loader.ftu'
    out.parent.mkdir(parents=True, exist_ok=True)
    source = screen(); blob = encode(source)
    assert decode(blob) == source
    out.write_bytes(blob)
