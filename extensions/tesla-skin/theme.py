"""Shared theme definition for the resource generator and layout compiler."""
import json
import os
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent


def load_theme(path=None):
    theme = json.loads(Path(path or os.environ.get('DASHBORNE_THEME', ROOT / 'theme.json')).read_text())
    if theme.get('schema') != 1 or theme.get('target_os') != '6.11.4':
        raise ValueError('Expected theme schema 1 for stock OS 6.11.4')
    if not re.fullmatch(r'[A-Za-z0-9_-]{1,63}', theme.get('font', '')):
        raise ValueError('Invalid font family')
    for palette in theme['palettes'].values():
        for key in ('bg', 'text', 'muted', 'disabled', 'card', 'selected', 'pressed', 'line'):
            color(palette[key])
    if set(theme['palettes']) != {'dash', 'light'}:
        raise ValueError('Both dash and light palettes are required')
    for key in ('accent','positive','accent_pressed','on_accent','switch_thumb'):color(theme[key])
    # Only visual properties: navigation, callbacks, IDs and data bindings stay native.
    allowed = {'fontSize', 'bold', 'family', 'colorTab', 'bgColorTab', 'backgroundColor',
               'backgroundPic', 'picTab', 'thumbPicTab', 'progressPic', 'position', 'textPosition'}
    for controls in theme.get('controls', {}).values():
        for properties in controls.values():
            if set(properties) - allowed:
                raise ValueError('Control overrides must contain visual properties only')
            for key, value in properties.items():
                if key == 'fontSize' and (type(value) is not int or not 1 <= value <= 256):
                    raise ValueError('fontSize must be an integer from 1 to 256')
                if key == 'bold' and type(value) is not bool:
                    raise ValueError('bold must be a boolean')
                if key in ('position', 'textPosition'):
                    if not isinstance(value, dict) or set(value) - {'left', 'top', 'width', 'height'}:
                        raise ValueError('Invalid control geometry')
                    for axis, number in value.items():
                        if type(number) is not int or not (-8192 if axis in ('left', 'top') else 0) <= number <= 8192:
                            raise ValueError('Control geometry is out of range')
                if key in ('colorTab', 'bgColorTab'):
                    if not isinstance(value, dict) or any(not re.fullmatch(r'color[0-4]', k) or type(v) is not int or not -1 <= v <= 0xffffffff for k, v in value.items()):
                        raise ValueError('Invalid state color table')
                if key == 'backgroundColor' and (type(value) is not int or not -1 <= value <= 0xffffffff):
                    raise ValueError('Invalid background color')
                if key in ('family', 'backgroundPic', 'progressPic') and (not isinstance(value, str) or len(value) > 200 or '\0' in value):
                    raise ValueError('Invalid resource or font name')
                if key in ('picTab', 'thumbPicTab') and (not isinstance(value, dict) or any(not re.fullmatch(r'pic[0-4]', k) or not isinstance(v, str) or len(v) > 200 or '\0' in v for k, v in value.items())):
                    raise ValueError('Invalid state image table')
    return theme


def color(value):
    if not isinstance(value, str) or not re.fullmatch(r'#[0-9a-fA-F]{6}', value):
        raise ValueError('Expected #RRGGBB color')
    return int(value[1:], 16)


THEME = load_theme()


def write_runtime_tokens():
    lines = ['/* Generated from theme.json. */', 'static const unsigned int theme_colors[2][7]={']
    for mode in ('dash', 'light'):
        p = {k: color(v) for k, v in THEME['palettes'][mode].items()}
        values = [p['bg'] | 0xff000000, p['text'], p['text'], p['muted'], p['muted'], p['muted'], p['disabled']]
        lines.append('{' + ','.join(f'0x{v:08x}u' for v in values) + '},')
    lines += ['};', '#define THEME_FONT ' + json.dumps(THEME['font'])]
    (ROOT / 'theme_tokens.inc').write_text('\n'.join(lines) + '\n')
