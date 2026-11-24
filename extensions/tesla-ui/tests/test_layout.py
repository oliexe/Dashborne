import sys
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
assert s['textview__limit_0bg']['id']==599314
assert s['textview__limit_1value']['id']==599315
assert s['textview__power_0line']['id']==599322
assert s['textview__power_1fill']['id']==599323
