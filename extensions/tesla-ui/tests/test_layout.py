import sys
import struct
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from generate import screen,decode,ROOT
s=screen()
assert decode((ROOT/'assets/cluster.ftu').read_bytes())==s
ids=set()
def check(node,parent):
    if 'id' in node:
        assert node['id'] not in ids
        ids.add(node['id'])
    p=node.get('position',parent)
    assert 0<=p['left'] and 0<=p['top']
    assert p['left']+p['width']<=parent['width']
    assert p['top']+p['height']<=parent['height']
    for key,value in node.items():
        if '__' in key:check(value,p)
    for value in [node.get('backgroundPic',''),*node.get('picTab',{}).values()]:
        if value:assert (ROOT/'assets'/Path(value).name).is_file()
check(s,dict(left=0,top=0,width=1920,height=480))
assert all(600000+i in ids and 601000+i in ids for i in range(414))
assert s['window__z_back']['button__back']['touchable']
assert s['window__z_preview']['button__preview']['touchable']
assert not s['textview__preview_status']['visible']
assert s['textview__limit_0bg']['id']==599314
assert s['textview__limit_1value']['id']==599315
assert s['textview__power_0line']['id']==599322
assert s['textview__power_1fill']['id']==599323
assert all(603000+i in ids for i in range(4))
order=sorted(s)
assert order.index('textview__car_00_road')<order.index('textview__car_02_fog')<order.index('textview__car_drive')
for side in ('left','right'):
    for state in ('turn','blind','alert'):
        header=(ROOT/'assets'/f'road-{side}-{state}.png').read_bytes()[:24]
        assert struct.unpack('>II',header[16:24])==(212,189)

# Every reachable panel combination must be packaged for both views.
for mask in range(64):
    for view,size in (('park',(600,360)),('drive',(600,360))):
        path=ROOT/'assets/body'/f'{view}-{mask:02x}.png'
        header=path.read_bytes()[:24]
        assert header[:8]==b'\x89PNG\r\n\x1a\n',path
        assert struct.unpack('>II',header[16:24])==size,path
