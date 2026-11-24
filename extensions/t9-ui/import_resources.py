"""Adapt an extracted T9 resource directory for the native Dashborne view."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import shutil
import sys

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / 'dashborne'))
from ui_codec import decode, encode

PREFIX = '../../../mnt/extsd/Dashborne/imports/t9-ui/'
def position(x, y, w, h):
    return dict(left=x, top=y, width=w, height=h)


def label(id, text, x, y, w, h, touch=False):
    return dict(id=id, text=text, position=position(x,y,w,h), fontSize=22,
                alignment=37, colorTab={'color0': 0xb5bbc4}, touchable=touch,
                backgroundColor=0x171a1e if touch else -1)


def adapt(source, style, assets):
    root = copy.deepcopy(source)
    # Keep decorative geometry and the supported readings. All control behavior
    # belongs to the extension, never to the imported application's callbacks.
    allowed = ({50001,50020,50025,50073,50010,50040,93001,50024,50008,50026,50027}
               if style == 0 else
               {50044,50001,50020,50025,50007,50034,50041,93001,130007})
    ids = ({50020:0,50073:1,50010:2,50040:3,50025:4} if style == 0 else
           {50020:0,50007:2,50041:3,50025:4,130007:5})
    def walk(node, key=''):
        if isinstance(node, list):
            return [walk(v,key) for v in node]
        if not isinstance(node, dict):
            if isinstance(node,str) and node.lower().endswith(('.png','.jpg','.jpeg','.bmp')):
                p = Path(node)
                if p.is_absolute() or '..' in p.parts or '\\' in node:
                    raise ValueError('Resource path escapes source')
                assets.add(node)
                return PREFIX + node
            return node
        result = {}
        for k,v in node.items():
            if isinstance(v,dict) and 'id' in v and '__' in k and not k.startswith('window__') and v['id'] not in allowed:
                continue
            result[k] = walk(v,k)
        if 'id' in node:
            old = node['id']
            result['id'] = 599910+style*20+ids[old] if old in ids else 1000000*(style+1)+old
            result['touchable'] = False
            result['visible'] = old in allowed or key.startswith('window__') or old == 0
            result['beepEnable'] = False
            if 'text' in result:
                result['text'] = '--' if old in ids else ''
            if old in ids and ids[old] == 3:
                result.pop('backgroundPic',None)
                result['fontSize']=32
                result['alignment']=37
                result['colorTab']={'color0':0xffffff}
            if key.startswith('circlebar__'):
                result['progress']=0
        return result
    result = walk(root)
    result['id']=119910+style
    result['visible']=style==0
    result.pop('resolution',None)
    # Controls excluded above cannot display sample warning lights or values.
    return result


def prepare(source, output):
    source, output = Path(source).resolve(), Path(output).resolve()
    if output.is_relative_to(REPO) or output.is_relative_to(source):
        raise ValueError('Choose an output outside the repository and source tree')
    if output.exists():
        raise ValueError('Output already exists; use a new directory')
    layouts = [decode((source/'ui'/name).read_bytes()) for name in ('dashsimple.ftu','dash5.ftu')]
    for layout in layouts:
        if layout.get('resolution') != {'width':1920,'height':480}:
            raise ValueError('Expected a 1920 x 480 T9 layout')
    assets = set()
    result = dict(id=0, backgroundColor=0, position=position(0,0,1920,480),
                  resolution={'width':1920,'height':480}, visible=True, hideTimeOut=-1)
    for style,layout in enumerate(layouts):
        result['window__style'+str(style)] = adapt(layout,style,assets)
    result['button__back']=label(299910,'Back',16,390,76,64,True)
    result['button__style']=label(299911,'Style',1820,390,84,64,True)
    result['textview__status']=label(599900,'T9 prototype',100,20,550,28)
    # Validate every referenced file before producing output. Imported fonts
    # would change the shared renderer, so this first port uses the host font.
    for name in assets:
        path=(source/'ui'/name).resolve()
        if not path.is_relative_to(source/'ui') or not path.is_file():
            raise ValueError('Missing or escaping resource: '+name)
    output.mkdir(parents=True)
    for name in sorted(assets):
        target=output/name;target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copyfile(source/'ui'/name,target)
    (output/'viewer.ftu').write_bytes(encode(result))
    manifest={str(p.relative_to(output)):hashlib.sha256(p.read_bytes()).hexdigest()
              for p in sorted(output.rglob('*')) if p.is_file()}
    (output/'import.json').write_text(json.dumps(manifest,indent=2)+'\n')
    return result


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('resources',type=Path)
    parser.add_argument('output',type=Path)
    args=parser.parse_args()
    prepare(args.resources,args.output)
    print('Prepared T9 layouts:',args.output)
