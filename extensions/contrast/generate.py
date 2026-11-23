"""Build an original settings panel and guarded additive FTU patch."""
import json
from pathlib import Path
import struct
import sys
import zlib
ROOT = Path(__file__).resolve().parent

def pos(x, y, w, h):
    return dict(left=x, top=y, width=w, height=h)

def label(id, text, x, y, w, h, size=26, color=0xffffff):
    return dict(id=id, text=text, fontSize=size, alignment=36,
                colorTab={'color0':color}, position=pos(x,y,w,h), touchable=False)

def panel():
    p=dict(id=119090, position=pos(700,0,1000,480), visible=False,
           backgroundColor=0x111111)
    p['textview__contrast_title']=label(599090,'Contrast',24,14,560,46,32)
    p['button__contrast_enable']=dict(label(299090,'Enable adjustments',650,14,326,54,24),
        touchable=True, backgroundColor=0x292929, alignment=37)
    p['textview__contrast_value']=label(599091,'Contrast: 50',24,85,860,36)
    p['textview__contrast_black']=label(599092,'Black level / luminance: 50',24,196,860,36)
    for id,key,y in [(919091,'contrast',127),(919092,'luminance',238)]:
        p['seekbar__'+key]=dict(id=id, position=pos(24,y,952,54), max=100,
            backgroundPic='dash/settings/slider_bg.png',
            progressPic='dash/settings/slider_fg.png',
            thumb=dict(normalPic='dash/settings/slider_thumb.png',size=dict(width=30,height=30)))
    for i,shade in enumerate((0,4,8,12,16,24,32,48,64,96,128,192,255)):
        p['textview__shade_'+str(i)]=dict(id=599100+i,position=pos(24+i*73,320,73,32),
            backgroundColor=shade*0x010101,touchable=False)
    p['textview__contrast_help']=label(599093,'Luminance shifts image levels; backlight stays unchanged.',24,362,952,32,20,0xa0a0a0)
    p['textview__contrast_status']=label(599094,'Original display values',24,414,610,36,20,0xa0a0a0)
    p['button__contrast_reset']=dict(label(299091,'Reset',742,407,234,54,24),
        touchable=True,backgroundColor=0x292929,alignment=37)
    return p

def build(base):
    offset,declared,packed=struct.unpack_from('<III',base,6)
    plain=zlib.decompress(base[offset+8:-6])
    assert base[:6]==b'ZKSW\xaa\x55' and base[-6:]==b'\x55\xaaZKSW'
    assert declared==len(plain)+8 and packed==len(base)-6-(offset+8)+16
    root=json.loads(plain)
    ids=[]
    def walk(v):
        if isinstance(v,dict):
            if 'id' in v: ids.append(v['id'])
            for c in v.values(): walk(c)
        elif isinstance(v,list):
            for c in v: walk(c)
    walk(root); old=set(ids); ids.clear(); walk(panel())
    assert len(ids)==len(set(ids)) and not old.intersection(ids)
    fragment=b',"window__dashborne_contrast":'+json.dumps(panel(),separators=(',',':')).encode()
    fields=(len(base),zlib.crc32(base),len(plain),zlib.crc32(plain),len(fragment),zlib.crc32(fragment))
    return b'DBP1'+struct.pack('<6I',*fields)+fragment

if __name__=='__main__':
    patch=build(Path(sys.argv[1]).read_bytes())
    (ROOT/'assets/panel.dbp').write_bytes(patch)
    print('Built original Contrast panel:',len(patch),'bytes')
