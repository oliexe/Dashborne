"""Compile original style changes against layouts freshly pulled through ADB."""
import argparse, copy, json, re, struct, sys, zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT.parents[1]/'dashborne'))
from ui_codec import decode
PALETTES={
    'dash':dict(bg=0x17181a,text=0xf4f4f4,muted=0xa2a5a9,disabled=0x686b70,card=0x242528,selected=0x3c3e42,line=0x3b3d41),
    'light':dict(bg=0xf1f2f3,text=0x171a20,muted=0x62666d,disabled=0xa4a7ac,card=0xffffff,selected=0xffffff,line=0xd7d9dc)}
BLUE=0x3e6ae1

def walk(node,path=()):
    if isinstance(node,dict):
        yield path,node
        for key,value in node.items():yield from walk(value,path+(key,))
    elif isinstance(node,list):
        for i,value in enumerate(node):yield from walk(value,path+(i,))

def transform(original,page,mode,resources):
    layout=copy.deepcopy(original);p=PALETTES[mode]
    for path,node in walk(layout):
        cap=node.get('caption','');kind=str(path[-1]) if path else ''
        native_preview = page=='widgetEdit' and ('window__27' in path or cap in ('STAT_VALUE','STAT_UNIT'))
        if 'fontSize' in node:
            size=node['fontSize']
            if page!='dash1act' and not native_preview:node['fontSize']=min(size,28) if size>=28 else size
            node['bold']=False
            # Native data labels can contain private unit glyphs in the stock font.
            if not native_preview and (page!='dash1act' or any(tag in cap for tag in ('SPEED_VAL','SPEED_UNIT','_TITLE_MEDIA_','_ARTIST_MEDIA_'))):
                node['family']='Inter-Regular'
        # Recolor only neutral UI text; preserve warnings, status and semantic colors.
        for key,value in list(node.get('colorTab',{}).items()):
            red=value>>16&255;green=value>>8&255;blue=value&255
            if max(red,green,blue)-min(red,green,blue)<12:
                node['colorTab'][key]=p['disabled'] if key=='color4' else p['text'] if max(red,green,blue)>200 else p['muted']
        for key,value in list(node.get('bgColorTab',{}).items()):
            if value in (0,0xff000000):node['bgColorTab'][key]=p['bg']
            elif value in (0xffffff,0xffffffff):node['bgColorTab'][key]=p['text']
        if any('btn_ok_bg' in str(v) or 'btn_dlg_confirm' in str(v) for v in node.get('picTab',{}).values()):
            node['colorTab']={key:0xffffff for key in node.get('colorTab',{'color0':0xffffff})}
        if 'backgroundColor' in node and node['backgroundColor'] in (0,0xff000000):node['backgroundColor']=p['bg']
        for key,value in list(node.items()):
            if isinstance(value,str) and value.startswith(('dash/','light/')):
                tail=value.split('/',1)[1]
                target=mode+'/'+tail
                if target in resources:node[key]='../../../mnt/extsd/Dashborne/extensions/tesla-skin/assets/ui/'+target
        if page=='awlsettings':
            if not path:node['backgroundColor']=p['bg']
            if cap=='TOPICS_LIST':
                node['rowSpacing']=9;node['position'].update(left=185,top=20,width=440)
                node['item']['position'].update(width=440,height=75)
                node['item']['textPosition'].update(left=24,width=392)
            if cap=='MENU_LBL':node['backgroundPic']='';node['visible']=False
            if cap=='BTN_BACK':node['text']='Back';node['fontSize']=26
        if page=='pageEdit' and cap in ('LBL_PC_InfoText','LBL_PA_InfoText'):
            node['fontSize']=22
        if page=='dash1act':
            if re.match(r'P[TMB]_G[12]_SPEED_VAL',cap):
                node['fontSize']=128;node['bold']=False
                node['position'].update(left=70,top=102,width=370,height=150)
                node['textPosition']={'left':0,'top':0,'width':370,'height':150}
            if re.match(r'P[TMB]_G[12]_SPEED_UNIT',cap):
                node['fontSize']=24;node['position'].update(left=155,top=256,width=200,height=36)
            if re.match(r'P[TMB]_GAUGE4_SPEED_VAL',cap):
                node['fontSize']=144;node['bold']=False
                node['position'].update(left=24,top=108,width=462,height=162)
            if re.match(r'P[TMB]_GAUGE4_SPEED_UNIT',cap):
                node['fontSize']=24;node['position'].update(left=155,top=276,width=200,height=36)
            if re.match(r'P[TMB]_Value1x[12]_',cap):node['fontSize']=56
            if re.match(r'P[TMB]_Unit1x[12]_',cap):node['fontSize']=22
            if '_ARTIST_MEDIA_' in cap:node['fontSize']=22
            if '_TITLE_MEDIA_' in cap:node['fontSize']=28;node['bold']=True
    return layout

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
        elif text[i]=='[':
            i=ws(i+1);index=0
            if text[i]!=']':
                while True:
                    i=ws(walk(i,path+(index,)));index+=1
                    if text[i]==']':break
                    if text[i]!=',':raise ValueError('Expected comma')
                    i=ws(i+1)
            i+=1
        else:
            _,i=decoder.raw_decode(text,i)
        spans[path]=(len(text[:start].encode()),len(text[start:i].encode()))
        return i
    walk(0,());return spans


def compile_delta(blob, changed):
    original=decode(blob)
    offset=struct.unpack_from('<I',blob,6)[0]
    plain=zlib.decompress(blob[offset+8:-6]);spans=object_spans(plain.decode())
    edits=[]
    def diff(a,b,path=()):
        if isinstance(b,list):
            for i,(x,y) in enumerate(zip(a,b)):
                if x!=y:diff(x,y,path+(i,))
            return
        added={k:v for k,v in b.items() if k not in a}
        if added:
            pos,length=spans[path]
            edits.append((pos+length-1,0,(','+json.dumps(added,separators=(',',':'))[1:-1]).encode()))
        for k,v in b.items():
            if k not in a or v==a[k]:continue
            if isinstance(v,(dict,list)):diff(a[k],v,path+(k,))
            else:
                pos,length=spans[path+(k,)];replacement=json.dumps(v,separators=(',',':')).encode()
                edits.append((pos,length,replacement))
    diff(original,changed);edits.sort()
    payload=b''.join(struct.pack('<III',p,n,len(v))+v for p,n,v in edits)
    header=b'ATP1'+struct.pack('<IIIIII',len(blob),zlib.crc32(blob),len(plain),zlib.crc32(plain),len(edits),zlib.crc32(payload))
    return header+payload,changed,len(edits)



def styles(layout,page):
    result=[]
    for path,n in walk(layout):
        if 'id' not in n or 'subItem' in path or 'radiobuttons' in path:continue
        # Radios are children with IDs too; handle below, unlike list-item templates.
        result.append((n.get('id',0),path,n))
    for path,n in walk(layout):
        if 'id' in n and 'radiobuttons' in path:result.append((n['id'],path,n))
    return result

def cstring(s):return json.dumps(s or '')
def write_styles(pages):
    out=['/* Original style metadata. IDs refer to controls created by the installed app. */']
    for page,variants in pages.items():
        for mode,layout in variants.items():
            out.append('static const struct style '+page+'_'+mode+'[] = {')
            for id,path,n in styles(layout,page):
                if page=='dash1act':
                    n={k:v for k,v in n.items() if k in ('colorTab',)}
                ct=n.get('colorTab',{});pt=n.get('picTab',{})
                colors=[str(ct.get('color'+str(i),ct.get('color0',0)))+'u' for i in range(5)]
                pics=[cstring(pt.get('pic'+str(i),'')) for i in range(5)]
                thumbs=n.get('thumbPicTab',{})
                out.append('{'+str(id)+','+str(bool(ct)).lower()+','+str('backgroundColor' in n).lower()+','+str(n.get('backgroundColor',0)&0xffffffff)+'u,{'+','.join(colors)+'},'+cstring(n.get('backgroundPic',''))+',{'+','.join(pics)+'},'+cstring(n.get('progressPic',''))+'},')
            out.append('};')
    (ROOT/'styles.inc').write_text('\n'.join(out)+'\n')

def prepare(directory):
    resources=set(json.loads((ROOT/'resource-sizes.json').read_text()))
    pages={}
    for page in ['dash1act','awlsettings','widgetEdit','pageEdit','dialog','btCommanderPair','UserIme']:
        blob=(directory/(page+'.ftu')).read_bytes();original=decode(blob);pages[page]={}
        for mode in PALETTES:
            changed=transform(original,page,mode,resources);pages[page][mode]=changed
            patch,_,count=compile_delta(blob,changed)
            (ROOT/'assets'/(page+'-'+mode+'.atp')).write_bytes(patch)
            print(page,mode,count,'edits',len(patch),'bytes')
    write_styles(pages)

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('adb_capture',type=Path)
    prepare(parser.parse_args().adb_capture)
