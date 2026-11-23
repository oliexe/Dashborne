import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from generate import screen,encode,decode,ROOT
s=screen()
assert decode((ROOT/'assets/viewer.ftu').read_bytes())==s
assert s['button__back']['touchable']
assert s['button__back']['position']['top']+s['button__back']['position']['height']<=80
assert not s['videoview__mirror']['visible']
assert s['videoview__mirror']['position']==dict(left=16,top=80,width=1888,height=384)
assert decode(encode(s))==s
controls=[s[key] for key in ('button__back','textview__title','textview__status','button__aspect','button__retry')]
for i,c in enumerate(controls):
    p=c['position']
    assert p['top']+p['height']<=80
    if i:
        before=controls[i-1]['position']
        assert before['left']+before['width']<=p['left']
assert s['button__aspect']['touchable']
