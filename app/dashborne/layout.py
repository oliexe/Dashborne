"""Original splash and configuration screen for the DASH's native UI runtime."""
from pathlib import Path
import sys
ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT.parent / 'dashboard'))
from layout import encode, decode

VERSION = (ROOT.parents[1] / 'VERSION').read_text().strip()


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


def switch(id, x, y):
    return dict(id=id, text='0', alignment=37, touchable=False,
                position=position(x, y, 72, 36),
                charsetTab=[dict(char=48+i, pic='../../../mnt/extsd/Dashborne/ui/toggle-'+name+'.png',
                                 size=dict(width=72, height=36))
                            for i, name in enumerate(('off', 'on', 'disabled'))])


def screen():
    root = dict(id=0, caption='Dashborne', backgroundColor=0x000000,
                position=position(0, 0, 1920, 480), resolution=dict(width=1920, height=480),
                visible=True, topmost=False, beepEnable=False, hideTimeout=-1)
    splash = dict(id=110001, position=position(0, 0, 1920, 480), visible=True)
    splash['button__1'] = dict(id=20001, position=position(0, 0, 1920, 480), touchable=True)
    splash['textview__2'] = dict(id=50010, position=position(840, 112, 240, 160), touchable=False,
                               backgroundPic='../../../mnt/extsd/Dashborne/ui/icon.png')
    splash['textview__3'] = label(50011, 'Tap screen to enter configuration', 560, 282, 800, 36, 22, 0xa4aabd, 37)
    config = dict(id=110002, position=position(0, 0, 1920, 480), visible=False, backgroundColor=0x000000)
    config['textview__4'] = dict(id=50012, position=position(170, 416, 60, 40), touchable=False,
                               backgroundPic='../../../mnt/extsd/Dashborne/ui/icon.png')
    config['textview__5'] = label(50013, 'DASHBORNE', 242, 416, 340, 30, 23, 0xf0f2ff)
    config['textview__6'] = label(50014, f'v{VERSION}  /  https://github.com/oliexe/Dashborne', 180, 449, 440, 20, 14, 0x79849a)
    for id, key, text, y in [(20006, 'patches', 'Patches', 40), (20007, 'adb', 'ADB', 116)]:
        config['button__nav_'+key] = button(id, text, 180, y, 400, 64)
        config['button__nav_'+key].update(backgroundColor=0x000000,
            textPosition=position(36, 0, 340, 64),
            picTab={'pic0': '../../../mnt/extsd/Dashborne/ui/nav-idle.png',
                    'pic1': '../../../mnt/extsd/Dashborne/ui/nav-pressed.png',
                    'pic2': '../../../mnt/extsd/Dashborne/ui/nav-selected.png'})
        config['textview__active_'+key] = dict(id=50020+(id-20006),
            position=position(196, y+22, 3, 20), backgroundColor=0x8eb5ff, touchable=False)
    config['textview__divider'] = dict(id=50017, position=position(624, 40, 1, 352),
                                     backgroundColor=0x202228, touchable=False)
    adb = dict(id=110003, position=position(680, 48, 1060, 348), visible=False)
    adb['textview__heading'] = label(50015, 'ADB', 24, 0, 500, 36, 25, 0xdce2ee)
    adb['button__enable'] = button(20002, 'Enable ADB', 0, 48, 1060, 72)
    adb['button__enable'].update(backgroundColor=0x000000, bgColorTab={'color1': 0x14161c})
    adb['button__enable']['textPosition'] = position(24, 0, 900, 72)
    adb['textview__toggle'] = switch(50016, 964, 66)
    adb['textview__status'] = label(50002, 'Disabled', 24, 146, 950, 32, 20, 0x9ba6bd)
    adb['textview__address'] = label(50003, '', 24, 186, 950, 44, 30, 0x8eb5ff)
    patches = dict(id=110004, position=position(680, 48, 1060, 348), visible=True)
    patches['textview__heading'] = label(50005, 'Patches', 24, 0, 340, 36, 25, 0xdce2ee)
    patches['textview__count'] = label(50018, '', 440, 3, 596, 30, 17, 0x79849a, 38)
    item = label(24000, '', 0, 0, 1060, 60, 22, 0xe6e9f1)
    item.update(touchable=True, textPosition=position(24, 3, 900, 28),
                bgColorTab={'color0': 0x000000, 'color1': 0x15171d},
                subItem=[label(24001, '', 24, 31, 900, 23, 16, 0x79849a),
                         switch(24002, 964, 12),
                         dict(id=24003, position=position(24, 59, 1012, 1),
                              backgroundColor=0x202228, touchable=False)])
    patches['listview__patches'] = dict(id=80001, position=position(0, 48, 1060, 300),
        rows=5, cols=1, rowSpacing=0, colSpacing=0, item=item,
        hasScrollbar=True, scrollbarColor=0x596173, autoRollback=True,
        dragMaxDis=12, beepEnable=False, backgroundColor=0x000000)
    patches['textview__empty'] = label(50019, 'No patches found on microSD', 24, 70, 1000, 36, 22, 0x79849a)
    config['window__adb'] = adb
    config['window__patches'] = patches
    config['textview__footer_line'] = dict(id=50022, position=position(180, 406, 1560, 1),
        backgroundColor=0x202228, touchable=False)
    config['textview__40'] = label(50004, '', 704, 418, 700, 48, 18, 0x79849a)
    config['button__43'] = label(20003, 'Continue booting', 1440, 418, 300, 48, 22, 0xffffff, 37)
    config['button__43'].update(touchable=True,
        picTab={'pic0': '../../../mnt/extsd/Dashborne/ui/continue.png',
                'pic1': '../../../mnt/extsd/Dashborne/ui/continue-pressed.png'})
    root['window__1'] = splash
    root['window__2'] = config
    return root


if __name__ == '__main__':
    out = ROOT / 'build/card/Dashborne/ui/loader.ftu'
    out.parent.mkdir(parents=True, exist_ok=True)
    source = screen(); blob = encode(source)
    assert decode(blob) == source
    out.write_bytes(blob)
    from switches import write_switches, write_boot_button
    write_switches(out.parent)
    write_boot_button(out.parent)
