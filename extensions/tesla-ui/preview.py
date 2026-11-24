"""Render native window test fixtures against the packaged layout. Requires Pillow."""
import copy
import json
import subprocess
import tempfile
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
from generate import ROOT,screen
OUT=ROOT.parents[1]/'dashborne/build/tesla-ui-previews'
def render(snapshot):
    controls={c['id']:c for c in snapshot['controls']}
    image=Image.new('RGBA',(1920,480),'black')
    fonts={}
    def draw_node(node,origin=(0,0)):
        node=copy.deepcopy(node);over=controls.get(node['id'],{})
        if 'position' in over:node['position']=over['position']
        if over.get('visible',-1)>=0:node['visible']=bool(over['visible'])
        if not node.get('visible',True):return
        if over.get('image'):node['backgroundPic']=over['image']
        if 'text' in node and 'text' in over:node['text']=over['text']
        if 'color' in over:node['backgroundColor']=over['color']
        p=node['position'];x,y=p['left']+origin[0],p['top']+origin[1];w,h=p['width'],p['height']
        draw=ImageDraw.Draw(image)
        if 'backgroundColor' in node:
            value=node['backgroundColor'];draw.rectangle((x,y,x+w-1,y+h-1),fill=f'#{value:06x}')
        path=node.get('backgroundPic') or node.get('picTab',{}).get('pic0')
        if path:
            asset=Image.open(ROOT/'assets'/Path(path).name).convert('RGBA')
            ip=node.get('iconPosition',dict(left=0,top=0,width=w,height=h))
            asset=asset.resize((ip['width'],ip['height']),Image.Resampling.LANCZOS)
            image.alpha_composite(asset,(x+ip['left'],y+ip['top']))
        if node.get('text'):
            size=node.get('fontSize',24)
            if size not in fonts:
                f=ImageFont.truetype(str(ROOT/'design/Inter.ttf'),size);f.set_variation_by_axes([28,400]);fonts[size]=f
            center=node.get('alignment')==37
            draw.text((x+w/2 if center else x,y+h/2),node['text'],anchor='mm' if center else 'lm',
                      font=fonts[size],fill=f"#{node['colorTab']['color0']:06x}")
        for key in sorted(node):
            if '__' in key:draw_node(node[key],(x,y))
    draw_node(screen());return image.convert('RGB')
if __name__=='__main__':
    OUT.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory() as temp:
        binary=str(Path(temp)/'fixtures')
        subprocess.run(['clang','-std=c11','-fno-builtin','-I'+str(ROOT.parents[1]/'dashborne/extensions'),
                        str(ROOT/'tests/test_window.c'),str(ROOT/'model.c'),str(ROOT/'graph.c'),'-o',binary],check=True)
        snapshots=subprocess.check_output([binary,'--preview'],text=True).splitlines()
    for row in snapshots:
        snapshot=json.loads(row);path=OUT/(snapshot['name']+'.png');render(snapshot).save(path);print(path)
        (OUT/(snapshot['name']+'.json')).write_text(json.dumps(snapshot,indent=2))
