import json
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from generate import ROOT,screen,encode,decode,package
layout=screen()
assert decode((ROOT/'assets/menu.ftu').read_bytes())==layout
spec=json.loads((ROOT/'dashboard.json').read_text())
assert (ROOT/'assets/dashboard.dbp').read_bytes()==package.compile_patch(spec,native_controls=True)
button=spec['control'];p=button['position']
assert p['left']>=1700 and p['top']>=400 and p['left']+p['width']<=1920 and p['top']+p['height']<=480
assert button['button__extra']['touchable']
assert spec['key'].startswith('window__zz')
assert not layout['listview__entries']['hasScrollbar']
assert len({v['id'] for v in layout.values() if isinstance(v,dict) and 'id' in v})==len([v for v in layout.values() if isinstance(v,dict) and 'id' in v])
