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
    for id, key, text, y in [(20006, 'patches', 'Extensions', 40), (20007, 'adb', 'ADB', 116), (20008, 'loader', 'Loader', 192), (20012, 'device', 'Device', 268), (20018, 'update', 'Update Dashborne', 344)]:
        config['button__nav_'+key] = button(id, text, 180, y, 400, 56)
        config['button__nav_'+key].update(backgroundColor=0x000000,
            textPosition=position(36, 0, 340, 56),
            picTab={'pic0': '../../../mnt/extsd/Dashborne/ui/nav-idle.png',
                    'pic1': '../../../mnt/extsd/Dashborne/ui/nav-pressed.png',
                    'pic2': '../../../mnt/extsd/Dashborne/ui/nav-selected.png'})
        config['textview__active_'+key] = dict(id={20006: 50020, 20007: 50021, 20008: 50035, 20012: 50050, 20018: 50200}[id],
            position=position(196, y+22, 3, 20), backgroundColor=0xc2c9d1, touchable=False)
    adb = dict(id=110003, position=position(680, 24, 1060, 376), visible=False,
               backgroundPic='../../../mnt/extsd/Dashborne/ui/content-panel.png')
    adb['textview__heading'] = dict(label(50015, 'ADB', 24, 16, 500, 36, 28, 0xf1f2f4), bold=True)
    adb['textview__header_line'] = dict(id=50024, position=position(24, 64, 1012, 1),
        backgroundColor=0x202228, touchable=False)
    for key, title, control_id, switch_id, y in [
        ('enable', 'Enable ADB', 20002, 50016, 72),
        ('developer', 'Developer Mode', 20015, 50066, 120),
    ]:
        adb['button__'+key] = button(control_id, title, 0, y, 1060, 48)
        adb['button__'+key].update(backgroundColor=0x07080a, bgColorTab={'color1': 0x1c2025})
        adb['button__'+key]['textPosition'] = position(24, 0, 900, 48)
        adb['textview__toggle_'+key] = switch(switch_id, 964, y+6)
    adb['button__network'] = dict(button(20017, 'Wi-Fi network', 0, 264, 256, 48),
        fontSize=22, textPosition=position(24, 0, 232, 48),
        bgColorTab={'color0': 0x07080a, 'color1': 0x1c2025})
    for key, title, value_id, label_id, y, size, color in [
        ('status', 'Status', 50002, 50028, 168, 20, 0xa4abb3),
        ('address', 'IP address', 50003, 50029, 216, 22, 0xc2c9d1),
    ]:
        adb['textview__label_'+key] = label(label_id, title, 24, y, 220, 48, 22, 0xe4e7eb)
        adb['textview__'+key] = label(value_id, '-', 256, y, 780, 48, size, color, 38)
    # Native non-touchable text views still consume hits. Each row segment is a
    # button, so tapping the name/value/chevron all opens the same editor.
    adb['button__network_value'] = dict(label(50027, '', 256, 264, 744, 48, 22, 0xd9dde2, 38),
        touchable=True, textPosition=position(0, 0, 736, 48))
    adb['button__network_arrow'] = dict(label(50076, '>', 1000, 264, 60, 48, 22, 0x858c94, 36),
        touchable=True)
    for index, y in enumerate((119, 167, 215, 263, 311)):
        adb['textview__row_line_'+str(index)] = dict(id=50070+index,
            position=position(24, y, 1012, 1), backgroundColor=0x202228, touchable=False)
    patches = dict(id=110004, position=position(680, 24, 1060, 376), visible=True,
                   backgroundPic='../../../mnt/extsd/Dashborne/ui/content-panel.png')
    patches['textview__heading'] = dict(label(50005, 'Extensions', 24, 16, 340, 36, 28, 0xf1f2f4), bold=True)
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
    patches['textview__empty'] = label(50019, 'No extensions found on microSD', 24, 94, 1000, 36, 22, 0x858c94)
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
    update = dict(id=110008, position=position(680, 24, 1060, 376), visible=False,
        backgroundPic='../../../mnt/extsd/Dashborne/ui/content-panel.png')
    update['textview__heading'] = dict(label(50201, 'Update Dashborne', 24, 16, 320, 36, 28, 0xf1f2f4), bold=True)
    def update_button(id, text, x, y, w, h, asset):
        return dict(label(id, text, x, y, w, h, 20, 0xe4e7eb, 37), touchable=True,
                    picTab={'pic0': f'../../../mnt/extsd/Dashborne/ui/update-{asset}.png',
                            'pic1': f'../../../mnt/extsd/Dashborne/ui/update-{asset}-pressed.png'})
    update['button__rollback'] = update_button(20303, 'Restore previous', 800, 16, 236, 36, 'restore')
    update['textview__header_line'] = dict(id=50205, position=position(24, 64, 1012, 1),
        backgroundColor=0x202228, touchable=False)
    update['button__network'] = dict(update_button(20300, 'Wi-Fi: choose network  >', 24, 76, 680, 44, 'network'),
        alignment=36, textPosition=position(16, 0, 648, 44))
    update['button__refresh'] = update_button(20301, 'Check Updates', 724, 76, 312, 44, 'check')
    update['textview__releases_heading'] = label(50206, 'Available releases', 24, 130, 500, 26, 18, 0xa4abb3)
    release_item = label(24030, '', 0, 0, 1060, 56, 21, 0xe4e7eb)
    release_item.update(touchable=True, textPosition=position(24, 0, 380, 56),
        bgColorTab={'color0': 0x07080a, 'color1': 0x1c2025, 'color2': 0x242930},
        subItem=[label(24031, '', 420, 0, 616, 56, 17, 0xa4abb3, 38),
                 dict(id=24032, position=position(24, 55, 1012, 1), backgroundColor=0x202228, touchable=False)])
    update['listview__releases'] = dict(id=80004, position=position(0, 160, 1060, 168),
        rows=3, cols=1, rowSpacing=0, colSpacing=0, item=release_item,
        hasScrollbar=True, scrollbarColor=0x626a73, autoRollback=True, dragMaxDis=12)
    update['textview__empty'] = label(50207, '', 24, 176, 1012, 64, 21, 0x858c94, 37)
    update['textview__status'] = label(50203, '', 24, 328, 1012, 40, 17, 0xc2c9d1)
    config['window__update'] = update
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
    # Draw navigation after page contents: native list damage can extend into
    # the sidebar while a page is refreshed.
    for key in list(config):
        if key.startswith(('button__nav_', 'textview__active_')):
            config[key] = config.pop(key)
    confirm = dict(id=110009, position=position(0, 0, 1920, 480), visible=False,
                   backgroundColor=0x000000)
    card = dict(id=110010, position=position(430, 52, 1060, 376),
                backgroundPic='../../../mnt/extsd/Dashborne/ui/content-panel.png')
    card['textview__title'] = dict(label(50300, '', 32, 24, 996, 40, 28, 0xf1f2f4), bold=True)
    card['textview__target'] = label(50301, '', 32, 72, 996, 28, 20, 0xa4abb3)
    for index, control_id in enumerate((50302, 50304, 50305)):
        card['textview__notes_'+str(index)] = label(control_id, '', 32, 112+36*index, 996, 32, 19, 0xd9dde2)
    card['textview__notice'] = label(50303, '', 32, 236, 996, 48, 18, 0xa4abb3)
    card['button__cancel'] = update_button(20304, 'Cancel', 32, 308, 280, 44, 'install')
    card['button__install'] = update_button(20302, 'Install & restart', 748, 308, 280, 44, 'install')
    confirm['window__card'] = card
    root['window__confirm'] = confirm
    progress = dict(id=110011, position=position(0, 0, 1920, 480), visible=False,
                    backgroundColor=0x000000)
    progress_card = dict(id=110012, position=position(430, 52, 1060, 376),
                         backgroundPic='../../../mnt/extsd/Dashborne/ui/content-panel.png')
    progress_card['textview__title'] = dict(label(50310, '', 32, 48, 996, 44, 28, 0xf1f2f4, 37), bold=True)
    progress_card['textview__status'] = label(50311, '', 32, 112, 996, 64, 22, 0xd9dde2, 37)
    for i in range(3):
        for state in ('off', 'on'):
            progress_card['textview__loading_'+state+'_'+str(i)] = dict(
                id=50320+i if state == 'on' else 50323+i,
                position=position(510+i*15, 208, 10, 10), touchable=False,
                visible=state == 'off' or i == 0,
                backgroundPic='../../../mnt/extsd/Dashborne/ui/loading-'+state+'.png')
    progress_card['textview__notice'] = label(50312, '', 32, 274, 996, 40, 18, 0xa4abb3, 37)
    progress['window__card'] = progress_card
    root['window__progress'] = progress
    root['window__1'] = splash
    root['window__2'] = config
    wifi = dict(id=110007, position=position(0, 0, 1920, 480), visible=False,
                backgroundColor=0x000000)
    wifi['textview__heading'] = dict(label(50100, 'Wi-Fi setup', 180, 24, 480, 40, 28, 0xf1f2f4), bold=True)
    wifi['textview__typing'] = label(50102, '', 700, 28, 1040, 36, 20, 0xa4abb3)
    for field, title, title_id, control_id, y in [
        ('ssid', 'Network name (SSID)', 50101, 20100, 82),
        ('password', 'Password', 50104, 20101, 188),
    ]:
        wifi['textview__'+field] = label(title_id, title, 180, y, 360, 30, 20, 0xa4abb3)
        wifi['button__'+field] = dict(button(control_id, '', 180, y+36, 480, 58),
            fontSize=22, bgColorTab={'color0': 0x171a1e, 'color1': 0x2b3139})
    wifi['button__reveal'] = dict(label(20109, 'Show', 568, 188, 92, 30, 18, 0xc2c9d1, 38), touchable=True)
    wifi['textview__error'] = label(50103, '', 180, 292, 480, 48, 19, 0xff6369)
    for id, text, x, primary in [(20103, 'Cancel', 180, False), (20102, 'Save', 430, True)]:
        wifi['button__'+text.lower()] = button(id, text, x, 356, 230, 58, primary)
    for i in range(40):
        key = label(20200+i, '', 700+(i%10)*104, 84+(i//10)*64, 96, 56, 24, 0xf1f2f4, 37)
        key.update(touchable=True, bgColorTab={'color0': 0x171a1e, 'color1': 0x454e59})
        wifi['button__key_'+str(i)] = key
    for id, text, x, width in [(20104, 'Shift', 700, 160), (20105, '#+=', 868, 128),
                              (20106, 'Space', 1004, 340), (20107, 'Delete', 1352, 192),
                              (20108, 'Clear', 1552, 180)]:
        wifi['button__control_'+str(id)] = dict(label(id, text, x, 356, width, 58, 20, 0xd9dde2, 37),
            touchable=True, bgColorTab={'color0': 0x242930, 'color1': 0x454e59})
    root['window__wifi'] = wifi
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
