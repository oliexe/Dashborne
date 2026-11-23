"""Author the theme's guarded layout delta from a local firmware reference."""
import argparse, copy, json, re, struct, sys, zlib
from pathlib import Path
ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT.parents[1] / 'app/dashboard'))
from layout import decode


def transform(node):
    """Keep identities, state, input bindings and page geometry; restyle content."""
    if not isinstance(node, dict): return
    cap = node.get('caption', '')
    if 'fontSize' in node:
        size = node['fontSize']
        node['fontSize'] = {14:16, 28:24, 30:24, 32:28, 35:28, 36:36, 42:36, 44:40, 46:48, 50:46}.get(size,size)
    if re.match(r'P[TMB]_Value1x[12]_',cap):
        node['fontSize']=56
        node['position'].update(left=16,top=45,width=218,height=74)
    if re.match(r'P[TMB]_Unit1x[12]_',cap):
        node['fontSize']=22;node['alignment']=37
        node['position'].update(left=16,top=113,width=218,height=30)
    if re.match(r'P[TMB]_Btn1x1_',cap):
        node['textPosition'].update(left=16,top=162,width=218,height=46)
        node['alignment']=37
    if re.match(r'P[TMB]_Btn1x2_',cap):
        node['textPosition'].update(left=16,top=397,width=218,height=44)
        node['alignment']=37
    if re.match(r'P[TMB]_Value2x1_',cap):
        node['fontSize']=52
        node['position']['height']=66
    if re.match(r'P[TMB]_G[12]_SPEED_VAL',cap):
        node['fontSize']=128;node['bold']=False
        node['position'].update(left=70,top=102,width=370,height=150)
        node['textPosition']={'left':0,'top':0,'width':370,'height':150}
    if re.match(r'P[TMB]_G[12]_SPEED_UNIT',cap):
        node['fontSize']=24;node['position'].update(left=155,top=256,width=200,height=36)
    if re.match(r'P[TMB]_G[12]_BTN',cap):
        node['fontSize']=20;node['bold']=False
        node['textPosition']={'left':24,'top':410,'width':462,'height':36}
    if re.match(r'P[TMB]_GAUGE4_SPEED_VAL',cap):
        node['fontSize']=144;node['bold']=False
        node['position'].update(left=24,top=108,width=462,height=162)
    if re.match(r'P[TMB]_GAUGE4_SPEED_UNIT',cap):
        node['fontSize']=24;node['position'].update(left=155,top=276,width=200,height=36)
    if '_TPMS_UNIT_' in cap:
        node['fontSize']=22
    if '_ARTIST_MEDIA_' in cap:
        node['fontSize']=22;node['bold']=False
    if '_TITLE_MEDIA_' in cap:
        node['fontSize']=28;node['bold']=True
    if '_DCLOCK_2x1_TIME' in cap:
        node['fontSize']=86
        node['position'].update(top=68,height=112)
    for value in node.values():
        if isinstance(value,dict):transform(value)


def object_spans(text):
    """Index direct member values without retaining a reference document."""
    decoder=json.JSONDecoder(); spans={}
    def ws(i):
        while i<len(text) and text[i].isspace():i+=1
        return i
    def walk(i,path):
        i=ws(i);start=i
        if text[i]=='{':
            i=ws(i+1)
            if text[i]!='}':
                while True:
                    key,end=decoder.raw_decode(text,i);i=ws(end)
                    if text[i]!=':':raise ValueError('Expected colon')
                    i=ws(walk(i+1,path+(key,)))
                    if text[i]=='}':break
                    if text[i]!=',':raise ValueError('Expected comma')
                    i=ws(i+1)
            i+=1
        else:
            _,i=decoder.raw_decode(text,i)
        spans[path]=(len(text[:start].encode()),len(text[start:i].encode()))
        return i
    walk(0,());return spans


def compile_delta(blob):
    original=decode(blob); changed=copy.deepcopy(original);transform(changed)
    offset=struct.unpack_from('<I',blob,6)[0]
    plain=zlib.decompress(blob[offset+8:-6]);spans=object_spans(plain.decode())
    edits=[]
    def diff(a,b,path=()):
        added={k:v for k,v in b.items() if k not in a}
        if added:
            pos,length=spans[path]
            edits.append((pos+length-1,0,(','+json.dumps(added,separators=(',',':'))[1:-1]).encode()))
        for k,v in b.items():
            if k not in a or v==a[k]:continue
            if isinstance(v,dict):diff(a[k],v,path+(k,))
            else:
                pos,length=spans[path+(k,)];replacement=json.dumps(v,separators=(',',':')).encode()
                edits.append((pos,length,replacement))
    diff(original,changed);edits.sort()
    payload=b''.join(struct.pack('<III',p,n,len(v))+v for p,n,v in edits)
    header=b'ATP1'+struct.pack('<IIIIII',len(blob),zlib.crc32(blob),len(plain),zlib.crc32(plain),len(edits),zlib.crc32(payload))
    return header+payload,changed,len(edits)


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('reference',type=Path);args=p.parse_args()
    patch,_,count=compile_delta(args.reference.read_bytes())
    out=ROOT/'assets/layout.atp';out.parent.mkdir(exist_ok=True);out.write_bytes(patch)
    print(f'{count} layout edits; {len(patch)} bytes')
