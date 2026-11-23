"""Original Extra menu layout and a version-guarded dashboard button patch."""
import importlib.util
import json
import sys
from pathlib import Path
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT.parents[1]/'app/dashboard'))
from layout import encode,decode
spec=importlib.util.spec_from_file_location('package',ROOT.parents[1]/'app/injector/package.py')
package=importlib.util.module_from_spec(spec);spec.loader.exec_module(package)
def pos(x,y,w,h):return dict(left=x,top=y,width=w,height=h)
def label(id,text,x,y,w,h,size=24,color=0xffffff):
    return dict(id=id,text=text,fontSize=size,alignment=36,colorTab={'color0':color},position=pos(x,y,w,h),touchable=False)
def screen():
    root=dict(id=0,caption='Extra',backgroundColor=0,position=pos(0,0,1920,480),
              resolution=dict(width=1920,height=480),visible=True,topmost=False,beepEnable=False,hideTimeout=-1)
    root['textview__title']=label(599200,'Extra',140,24,450,60,34)
    root['textview__divider']=dict(id=599201,position=pos(620,108,1,270),backgroundColor=0x292d34,touchable=False)
    item=label(249200,'',0,0,440,72,24)
    item.update(touchable=True,textPosition=pos(18,0,404,72),
                bgColorTab={'color0':0x000000,'color1':0x23272e,'color2':0x23272e})
    root['listview__entries']=dict(id=809200,position=pos(140,108,440,288),
        rows=4,cols=1,rowSpacing=0,colSpacing=0,item=item,hasScrollbar=True,
        scrollbarColor=0x596173,autoRollback=True,dragMaxDis=12,beepEnable=False,backgroundColor=0)
    root['textview__entry']=label(599202,'Extensions',700,120,1000,56,30)
    root['textview__description']=label(599203,'',700,190,1000,92,23,0xa0a7b4)
    root['textview__empty']=label(599204,'Enable extensions in Dashborne to see them here.',700,190,1000,92,23,0xa0a7b4)
    root['button__open']=dict(label(299201,'Open',700,310,240,64),touchable=True,
        alignment=37,bgColorTab={'color0':0x23272e,'color1':0x383e48})
    root['button__back']=dict(label(299202,'< Back',1740,385,164,80),touchable=True,alignment=37)
    return root
if __name__=='__main__':
    (ROOT/'assets').mkdir(exist_ok=True)
    data=screen();blob=encode(data);assert decode(blob)==data
    (ROOT/'assets/menu.ftu').write_bytes(blob)
    (ROOT/'assets/dashboard.dbp').write_bytes(package.compile_patch(json.loads((ROOT/'dashboard.json').read_text()),native_controls=True))
