"""Compile original style changes against layouts freshly pulled through ADB."""
import argparse, copy, json, re, struct, sys, zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT.parents[1]/'dashborne'))
from ui_codec import decode
sys.path.insert(0,str(ROOT))
from theme import THEME, color, write_runtime_tokens
PALETTES={mode:{k:color(v) for k,v in palette.items()} for mode,palette in THEME['palettes'].items()}
BLUE=color(THEME['accent'])
PAGES=['dash1act','awlsettings','widgetEdit','pageEdit','dialog','btCommanderPair','UserIme',
       'queryUser','production','phoneupdate','blefailure','screensaver']

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
                node['family']=THEME['font']
        # Recolor only neutral UI text; preserve warnings, status and semantic colors.
        for key,value in list(node.get('colorTab',{}).items()):
            red=value>>16&255;green=value>>8&255;blue=value&255
            if max(red,green,blue)-min(red,green,blue)<12:
                node['colorTab'][key]=p['disabled'] if key=='color4' else p['text'] if max(red,green,blue)>200 else p['muted']
        for key,value in list(node.get('bgColorTab',{}).items()):
            if value in (0,0xff000000):node['bgColorTab'][key]=p['bg']
            elif value in (0xffffff,0xffffffff):node['bgColorTab'][key]=p['text']
        if any('btn_ok_bg' in str(v) or 'btn_dlg_confirm' in str(v) for v in node.get('picTab',{}).values()):
            node['colorTab']={key:color(THEME['on_accent']) for key in node.get('colorTab',{'color0':0xffffff})}
        if cap=='RESTORE_DEFAULTS_BTN':node['colorTab']={'color0':p['text'],'color1':p['muted']}
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
        if page=='widgetEdit':
            if cap=='GAUGE_C':node['position'].update(left=0,top=0)
            if cap=='GC_MAIN_VAL':
                node['fontSize']=94;node['position'].update(left=60,top=152,width=390,height=126)
            if cap=='GC_MAIN_UNIT':
                node['fontSize']=23;node['position'].update(left=155,top=278,width=200,height=32)
        if page=='pageEdit' and cap in ('LBL_PC_InfoText','LBL_PA_InfoText'):
            node['fontSize']=22
        if page=='dash1act':
            if re.fullmatch(r'P[TMB]_HBAR_NAV',cap):
                arrow='../../../mnt/extsd/Dashborne/extensions/tesla-skin/assets/ui/'+mode+'/nav-progress-arrow.png'
                node['thumbPicTab']={'pic0':arrow,'pic1':arrow,'pic4':arrow}
                node['position'].update(top=114,height=14)
            # Reserve separate bands for mode, value, units, gear and secondary data.
            if re.match(r'P[TMB]_G[123]_BTN$',cap):
                node['fontSize']=22
                node['textPosition']={'left':40,'top':96,'width':430,'height':40}
            if re.match(r'P[TMB]_G3_ARC$',cap):
                node['backgroundPic']='../../../mnt/extsd/Dashborne/extensions/tesla-skin/assets/ui/'+mode+'/gauges/3/g3_face.png'
            if re.match(r'P[TMB]_G[123]_(SPEED|POWER)_VAL$',cap):
                node['fontSize']=128 if '_G1_' in cap else 94;node['bold']=False
                node['position'].update(left=60,top=152,width=390,height=126)
                node['textPosition']={'left':0,'top':0,'width':390,'height':126}
                node['family']=THEME['font']
            if re.match(r'P[TMB]_G[123]_(SPEED|POWER)_UNIT$',cap):
                node['fontSize']=23;node['position'].update(left=155,top=278,width=200,height=32)
            if re.match(r'P[TMB]_G[12]_GEAR_(ROLL|STRIP)$',cap):
                node['position']['top']=322
            if re.match(r'P[TMB]_GAUGE4_SPEED_VAL$',cap):
                node['fontSize']=128;node['bold']=False
                node['position'].update(left=24,top=152,width=462,height=126)
                node['textPosition']={'left':0,'top':0,'width':462,'height':126}
            if re.match(r'P[TMB]_GAUGE4_SPEED_UNIT$',cap):
                node['fontSize']=23;node['position'].update(left=155,top=278,width=200,height=32)
            if re.match(r'P[TMB]_TPMS_(FL|FR|RL|RR)_1x1$',cap):
                right=('_FR_' in cap or '_RR_' in cap);rear=('_RL_' in cap or '_RR_' in cap)
                node['fontSize']=28
                node['position'].update(left=158 if right else 8,top=140 if rear else 38,width=84,height=44)
            if re.match(r'P[TMB]_TPMS_BTN_1x1$',cap):
                node['fontSize']=18
                node['textPosition']={'left':0,'top':192,'width':250,'height':28}
            if re.match(r'P[TMB]_TPMS_VAL_',cap):node['fontSize']=42
            if re.match(r'P[TMB]_TPMS_UNIT_',cap):node['fontSize']=22
            if re.match(r'P[TMB]_Value1x[12]_',cap):node['fontSize']=56
            if re.match(r'P[TMB]_Unit1x[12]_',cap):node['fontSize']=22
            if '_ARTIST_MEDIA_' in cap:node['fontSize']=22
            if '_TITLE_MEDIA_' in cap:node['fontSize']=28;node['bold']=True
        overrides=THEME.get('controls',{}).get(page,{}) if ('id' in node or 'caption' in node) else {}
        for selector in ('*',cap,str(node.get('id',''))):
            for key,value in overrides.get(selector,{}).items():
                if isinstance(value,dict) and isinstance(node.get(key),dict):node[key].update(copy.deepcopy(value))
                else:node[key]=copy.deepcopy(value)
    if page=='dash1act':
        # Native digital gauges have no horizontal power controls. Add controls
        # to their existing windows so paging, editing and alpha remain native.
        for path,node in list(walk(layout)):
            cap=node.get('caption','')
            match=re.fullmatch(r'P([TMB])_(GAUGE1|GAUGE4)',cap)
            if not match:continue
            page_index='TMB'.index(match[1]);gauge=0 if match[2]=='GAUGE1' else 1
            base=620100+page_index*20+gauge*5
            y=356 if gauge==0 else 330
            for key,offset,x,width,height,color_value in [
                ('track',0,90,330,2,p['line']),('value',1,255,1,4,p['muted']),
                ('zero',2,254,2,8,p['muted'])]:
                node['textview__tesla_power_'+key]={
                    'id':base+offset,'caption':'TESLA_POWER_'+key.upper(),
                    'touchable':False,'visible':key!='value','backgroundColor':color_value,
                    'position':{'left':x,'top':y-(3 if key=='zero' else 1 if key=='value' else 0),
                                'width':width,'height':height}}
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
    # FlyThings reads these JSON colors through Json::Value::asInt. Pass RGB
    # here; direct C setters receive opaque ARGB separately in styles.inc.
    for _,node in walk(changed):
        if 'backgroundColor' in node and not -(1<<31)<=node['backgroundColor']<(1<<31):
            raise ValueError('backgroundColor exceeds the native signed JSON range')
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
            edits.append((pos+length-1,0,((',' if a else '')+json.dumps(added,separators=(',',':'))[1:-1]).encode()))
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
                if n.get('caption','').startswith('TESLA_POWER_'):
                    n=dict(n,backgroundColor=0xff000000|n['backgroundColor'])
                if page=='dash1act':
                    n={k:v for k,v in n.items() if k=='colorTab' or (k=='backgroundColor' and n.get('caption','').startswith('TESLA_POWER_')) or (k=='backgroundPic' and any(tag in n.get('caption','') for tag in ('_DEST_ICO_NAV','_G3_ARC')))}
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
    for page in PAGES:
        blob=(directory/(page+'.ftu')).read_bytes();original=decode(blob);pages[page]={}
        for mode in PALETTES:
            changed=transform(original,page,mode,resources);pages[page][mode]=changed
            patch,_,count=compile_delta(blob,changed)
            (ROOT/'assets'/(page+'-'+mode+'.atp')).write_bytes(patch)
            print(page,mode,count,'edits',len(patch),'bytes')
    write_styles(pages)
    write_runtime_tokens()

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('adb_capture',type=Path)
    prepare(parser.parse_args().adb_capture)
