import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from generate import screen,encode,decode,ROOT
s=screen()
assert decode((ROOT/'assets/viewer.ftu').read_bytes())==s
assert not s['videoview__mirror']['visible']
assert s['videoview__mirror']['position']==dict(left=0,top=0,width=1920,height=480)
assert decode(encode(s))==s
assert not any(k.startswith('textview__') for k in s)  # No permanent header.
for name in ('back','aspect','retry'):
    key='window__z_'+name
    panel=s[key]; button=panel['button__icon']; p=panel['position']
    assert key>'window__waiting' and key>'videoview__mirror'
    assert button['touchable'] and not button.get('text')
    assert p['width']==80 and p['height']==80
    assert 0<=p['left']<=1840 and 0<=p['top']<=400
    for value in button['picTab'].values():
        assert (ROOT/'assets'/Path(value).name).is_file()
assert s['window__z_back']['position']['top']>=380
assert not s['window__z_hint']['visible']

chooser=s['window__selector']
assert chooser['button__AirPlay']['selected']
assert not chooser['button__Miracast'].get('selected')
assert chooser['button__start']['text']=='Start mirroring'
ids=set()
def check(node,parent):
    if not isinstance(node,dict):return
    if 'id' in node:
        assert node['id'] not in ids
        ids.add(node['id'])
    if 'position' in node:
        p=node['position']
        assert 0<=p['left'] and 0<=p['top']
        assert p['left']+p['width']<=parent['width']
        assert p['top']+p['height']<=parent['height']
        parent=p
    for key,value in node.items():
        if '__' in key:check(value,parent)
check(s,dict(width=1920,height=480))
