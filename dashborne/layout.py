"""Original splash and configuration screen for the DASH's native UI runtime."""
from pathlib import Path
import sys
ROOT = Path(__file__).resolve().parent
from ui_codec import encode, decode

VERSION = (ROOT.parent / 'VERSION').read_text().strip()


def position(x, y, w, h):
    return dict(left=x, top=y, width=w, height=h)


def label(id, text, x, y, w, h, size=24, color=0xd9dde2, align=36):
    return dict(id=id, text=text, fontSize=size, alignment=align,
                colorTab={'color0': color}, position=position(x, y, w, h), touchable=False)


def button(id, text, x, y, w, h, primary=False):
    p = label(id, text, x, y, w, h, color=0xffffff if primary else 0xd9dde2)
    p.update(touchable=True, backgroundColor=0x5b6673 if primary else 0x171a1e,
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
    splash['textview__2'] = dict(id=50010, position=position(856, 133, 208, 208), touchable=False,
                               backgroundPic='../../../mnt/extsd/Dashborne/ui/icon.png')
    splash['textview__3'] = label(50011, 'Tap screen to enter boot option', 560, 434, 800, 24, 16, 0x858c94, 37)
    for i in range(3):
        for state in ('off', 'on'):
            splash['textview__loading_'+state+'_'+str(i)] = dict(
                id=50060+i if state == 'on' else 50063+i,
                position=position(940+i*15, 322, 10, 10), touchable=False,
                visible=state == 'off' or i == 0,
                backgroundPic='../../../mnt/extsd/Dashborne/ui/loading-'+state+'.png')
    config = dict(id=110002, position=position(0, 0, 1920, 480), visible=False, backgroundColor=0x000000)
    config['textview__4'] = dict(id=50012, position=position(170, 404, 60, 60), touchable=False,
                               backgroundPic='../../../mnt/extsd/Dashborne/ui/icon.png')
    config['textview__5'] = label(50013, 'DASHBORNE', 242, 416, 340, 30, 23, 0xf1f2f4)
    config['textview__6'] = label(50014, f'v{VERSION}  /  https://github.com/oliexe/Dashborne', 180, 449, 440, 20, 14, 0x858c94)
    for id, key, text, y in [(20006, 'patches', 'Patches', 40), (20007, 'adb', 'ADB', 116), (20008, 'loader', 'Loader', 192), (20012, 'device', 'Device', 268)]:
        config['button__nav_'+key] = button(id, text, 180, y, 400, 64)
        config['button__nav_'+key].update(backgroundColor=0x000000,
            textPosition=position(36, 0, 340, 64),
            picTab={'pic0': '../../../mnt/extsd/Dashborne/ui/nav-idle.png',
                    'pic1': '../../../mnt/extsd/Dashborne/ui/nav-pressed.png',
                    'pic2': '../../../mnt/extsd/Dashborne/ui/nav-selected.png'})
        config['textview__active_'+key] = dict(id={20006: 50020, 20007: 50021, 20008: 50035, 20012: 50050}[id],
            position=position(196, y+22, 3, 20), backgroundColor=0xc2c9d1, touchable=False)
    adb = dict(id=110003, position=position(680, 24, 1060, 376), visible=False,
               backgroundPic='../../../mnt/extsd/Dashborne/ui/content-panel.png')
    adb['textview__heading'] = dict(label(50015, 'ADB', 24, 16, 500, 36, 28, 0xf1f2f4), bold=True)
    adb['textview__header_line'] = dict(id=50024, position=position(24, 64, 1012, 1),
        backgroundColor=0x202228, touchable=False)
    adb['button__enable'] = button(20002, 'Enable ADB', 0, 72, 1060, 64)
    adb['button__enable'].update(backgroundColor=0x07080a, bgColorTab={'color1': 0x1c2025})
    adb['button__enable']['textPosition'] = position(24, 0, 900, 64)
    adb['textview__toggle'] = switch(50016, 964, 86)
    for key, title, value_id, label_id, y, size, color in [
        ('status', 'Status', 50002, 50028, 136, 20, 0xa4abb3),
        ('address', 'IP address', 50003, 50029, 200, 22, 0xc2c9d1),
        ('network', 'Wi-Fi network', 50027, 50030, 264, 22, 0xd9dde2),
    ]:
        adb['textview__label_'+key] = label(label_id, title, 24, y, 220, 64, 22, 0xe4e7eb)
        adb['textview__'+key] = label(value_id, '-', 256, y, 780, 64, size, color, 38)
    for index, y in enumerate((135, 199, 263, 327)):
        adb['textview__row_line_'+str(index)] = dict(id=50031+index,
            position=position(24, y, 1012, 1), backgroundColor=0x202228, touchable=False)
    patches = dict(id=110004, position=position(680, 24, 1060, 376), visible=True,
                   backgroundPic='../../../mnt/extsd/Dashborne/ui/content-panel.png')
    patches['textview__heading'] = dict(label(50005, 'Patches', 24, 16, 340, 36, 28, 0xf1f2f4), bold=True)
    patches['textview__header_line'] = dict(id=50026, position=position(24, 64, 1012, 1),
        backgroundColor=0x202228, touchable=False)
    patches['textview__count'] = label(50018, '', 440, 20, 596, 28, 17, 0x858c94, 38)
    item = label(24000, '', 0, 0, 1060, 56, 22, 0xe4e7eb)
    item.update(touchable=True, textPosition=position(24, 1, 900, 28),
                bgColorTab={'color0': 0x07080a, 'color1': 0x1c2025},
                subItem=[label(24001, '', 24, 27, 900, 23, 16, 0x858c94),
                         switch(24002, 964, 10),
                         dict(id=24003, position=position(24, 55, 1012, 1),
                              backgroundColor=0x202228, touchable=False)])
    patches['listview__patches'] = dict(id=80001, position=position(0, 72, 1060, 280),
        rows=5, cols=1, rowSpacing=0, colSpacing=0, item=item,
        hasScrollbar=True, scrollbarColor=0x626a73, autoRollback=True,
        dragMaxDis=12, beepEnable=False, backgroundColor=0x07080a)
    patches['textview__empty'] = label(50019, 'No patches found on microSD', 24, 94, 1000, 36, 22, 0x858c94)
    browser = dict(id=110005, position=position(680, 24, 1060, 376), visible=False,
        backgroundPic='../../../mnt/extsd/Dashborne/ui/content-panel.png')
    browser['textview__heading'] = dict(label(50040, 'Loader', 24, 16, 500, 36, 28, 0xf1f2f4), bold=True)
    browser['textview__header_line'] = dict(id=50043, position=position(24, 64, 1012, 1),
        backgroundColor=0x202228, touchable=False)
    browser['textview__path'] = label(50041, '/mnt/extsd', 24, 68, 768, 40, 18, 0xa4abb3)
    for id, key, text, x, w in [(20009, 'up', 'Up', 804, 80), (20010, 'refresh', 'Refresh', 900, 136)]:
        browser['button__'+key] = dict(label(id, text, x, 68, w, 40, 18, 0xc2c9d1, 37),
            touchable=True, bgColorTab={'color0': 0x07080a, 'color1': 0x171a1e})
    file_item = label(24010, '', 0, 0, 1060, 50, 20, 0xe4e7eb)
    file_item.update(touchable=True, textPosition=position(24, 0, 748, 50),
        bgColorTab={'color0': 0x07080a, 'color1': 0x1c2025, 'color2': 0x171a1e},
        subItem=[label(24011, '', 796, 0, 240, 50, 16, 0x858c94, 38),
                 dict(id=24012, position=position(24, 49, 1012, 1), backgroundColor=0x202228, touchable=False)])
    browser['listview__files'] = dict(id=80002, position=position(0, 112, 1060, 200),
        rows=4, cols=1, rowSpacing=0, colSpacing=0, item=file_item,
        hasScrollbar=True, scrollbarColor=0x626a73, autoRollback=True,
        dragMaxDis=12, beepEnable=False, backgroundColor=0x07080a)
    browser['textview__status'] = label(50042, 'Select an app', 24, 324, 420, 32, 17, 0xa4abb3)
    browser['button__autolaunch'] = dict(label(20014, 'Autolaunch', 464, 320, 328, 40, 18),
        touchable=True, textPosition=position(12, 0, 220, 40),
        bgColorTab={'color0': 0x07080a, 'color1': 0x171a1e})
    browser['textview__autolaunch_toggle'] = switch(50044, 708, 322)
    browser['button__load'] = dict(label(20011, 'Load app', 816, 320, 220, 40, 20, 0xffffff, 37),
        touchable=True, visible=False,
        picTab={'pic0': '../../../mnt/extsd/Dashborne/ui/continue.png',
                'pic1': '../../../mnt/extsd/Dashborne/ui/continue-pressed.png'})
    device = dict(id=110006, position=position(680, 24, 1060, 376), visible=False,
        backgroundPic='../../../mnt/extsd/Dashborne/ui/content-panel.png')
    device['textview__heading'] = dict(label(50051, 'Device', 24, 16, 500, 36, 28, 0xf1f2f4), bold=True)
    device['textview__header_line'] = dict(id=50052, position=position(24, 64, 1012, 1),
        backgroundColor=0x202228, touchable=False)
    device['button__refresh'] = dict(label(20013, 'Refresh', 900, 16, 136, 36, 18, 0xc2c9d1, 37),
        touchable=True, bgColorTab={'color0': 0x07080a, 'color1': 0x171a1e})
    device_item = label(24020, '', 0, 0, 1060, 56, 22, 0xe4e7eb)
    device_item.update(touchable=True, textPosition=position(24, 0, 340, 56),
        bgColorTab={'color0': 0x07080a, 'color1': 0x07080a},
        subItem=[label(24021, '', 380, 0, 656, 56, 18, 0xa4abb3, 38),
                 dict(id=24022, position=position(24, 55, 1012, 1), backgroundColor=0x202228, touchable=False)])
    device['listview__information'] = dict(id=80003, position=position(0, 72, 1060, 280),
        rows=5, cols=1, rowSpacing=0, colSpacing=0, item=device_item,
        hasScrollbar=True, scrollbarColor=0x626a73, autoRollback=True,
        dragMaxDis=12, beepEnable=False, backgroundColor=0x07080a)
    config['window__device'] = device
    config['window__browser'] = browser
    config['window__adb'] = adb
    config['window__patches'] = patches
    config['textview__footer_line'] = dict(id=50022, position=position(180, 406, 1560, 1),
        backgroundColor=0x202228, touchable=False)
    config['textview__40'] = dict(label(50004, '', 704, 418, 700, 48, 18, 0xff6369), bold=True)
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
    from switches import write_switches, write_boot_button, write_loading
    write_switches(out.parent)
    write_loading(out.parent)
    write_boot_button(out.parent)
