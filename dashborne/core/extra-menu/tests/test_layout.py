import json
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from generate import ROOT,screen,decode
layout=screen()
assert decode((ROOT/'assets/menu.ftu').read_bytes())==layout
spec=json.loads((ROOT/'dashboard.json').read_text())
assert json.dumps({spec['key']:spec['control']},separators=(',',':')) == json.loads((ROOT/'entry.inc').read_text().split('[]=\n')[1].rstrip(';\n'))
button=spec['control'];p=button['position']
assert p['left']>=1700 and p['top']>=400 and p['left']+p['width']<=1920 and p['top']+p['height']<=480
assert button['button__extra']['touchable']
assert spec['key'].startswith('window__zz')
assert not layout['listview__entries']['hasScrollbar']
assert len({v['id'] for v in layout.values() if isinstance(v,dict) and 'id' in v})==len([v for v in layout.values() if isinstance(v,dict) and 'id' in v])

assert button['visible'] is False
assert button['backgroundColor'] == -1
entry=button['button__extra']
assert entry['backgroundColor'] == -1 and 'bgColorTab' not in entry
assert set(entry['picTab'].values()) == {'../../../mnt/extsd/Dashborne/ui/icon.png'}
assert entry['iconPosition']['width'] == entry['iconPosition']['height']
