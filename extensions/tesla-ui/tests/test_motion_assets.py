import re,struct
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
rows=re.findall(r'\{"([^"]+)",(\d+),(\d+),(\d+),(\d+)\}',(ROOT/'motion_data.h').read_text())
assert len(rows)==347
camera_bytes=0
preloaded_bytes=0
for i,(name,x,y,w,h) in enumerate(rows):
    x,y,w,h=map(int,(x,y,w,h))
    assert 0<x and 0<y and x+w<600 and y+h<360,name
    data=(ROOT/'assets/motion'/name).read_bytes()
    assert data[:8]==b'\x89PNG\r\n\x1a\n'
    assert struct.unpack('>II',data[16:24])==(w,h)
    if i<65:camera_bytes+=w*h*4
    if i<65 or i>=193:preloaded_bytes+=w*h*4
assert camera_bytes<24*1024*1024,camera_bytes
for p in (ROOT/'assets/type').glob('*.png'):
    w,h=struct.unpack('>II',p.read_bytes()[16:24]);preloaded_bytes+=w*h*4
assert preloaded_bytes<64*1024*1024,preloaded_bytes
assert rows[0][0]=='camera-00.png' and rows[64][0]=='camera-64.png'
assert rows[65][0]=='park-00.png' and rows[192][0]=='drive-3f.png'
assert rows[193][0]=='park-hinge-01-01.png' and rows[346][0]=='drive-hinge-3f-11.png'
print('Vehicle camera pixels:',camera_bytes,'bytes; preloaded pixels:',preloaded_bytes,'bytes')
