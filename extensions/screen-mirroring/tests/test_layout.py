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
