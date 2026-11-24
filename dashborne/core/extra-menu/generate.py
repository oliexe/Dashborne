"""Original Extra menu layout and a version-guarded dashboard button patch."""
import importlib.util
import json
import sys
from pathlib import Path
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT.parents[1]))
from ui_codec import encode,decode
spec=importlib.util.spec_from_file_location('package',ROOT.parents[1]/'patches/package.py')
package=importlib.util.module_from_spec(spec);spec.loader.exec_module(package)
def pos(x,y,w,h):return dict(left=x,top=y,width=w,height=h)
def label(id,text,x,y,w,h,size=24,color=0xffffff):
    return dict(id=id,text=text,fontSize=size,alignment=36,colorTab={'color0':color},position=pos(x,y,w,h),touchable=False)
def screen():
    root=dict(id=0,caption='Extra',backgroundColor=0,position=pos(0,0,1920,480),
              resolution=dict(width=1920,height=480),visible=True,topmost=False,beepEnable=True,hideTimeout=-1)
    # Use the installed Settings visual vocabulary and column geometry.
    item=label(249200,'',0,0,480,75,28)
    item.update(touchable=True,textPosition=pos(24,0,456,75),subItem=[],
                picTab={'pic0':'dash/settings/listitem_bg_n.png',
                        'pic2':'dash/settings/listitem_bg_s.png',
                        'pic4':'dash/settings/listitem_bg_i.png'})
    root['listview__entries']=dict(id=809200,position=pos(185,20,480,450),
        rows=5,cols=1,rowSpacing=15,colSpacing=0,item=item,hasScrollbar=False,
        autoRollback=True,dragMaxDis=12,beepEnable=True,backgroundColor=0)
    root['textview__menu']=dict(id=599200,position=pos(122,270,38,186),
        backgroundPic='../../../mnt/extsd/Dashborne/assets/extra-menu/tweaks.png',touchable=False)
    panel=dict(id=119201,position=pos(700,20,1000,436),
               backgroundPic='dash/settings/unpair_bg.png')
    panel['textview__entry']=dict(label(599202,'Extra',30,22,940,64,32),bold=True)
    panel['textview__description']=label(599203,'',30,108,940,100,24,0x838383)
    panel['textview__empty']=label(599204,'Enable extensions in Dashborne to see them here.',30,108,940,100,24,0x838383)
    panel['button__open']=dict(label(299201,'Open',30,256,300,75,28),touchable=True,
        alignment=37,beepEnable=True,
        picTab={'pic0':'dash/settings/btn_con_n.png','pic1':'dash/settings/btn_con_p.png'})
    root['window__content']=panel
    root['button__back']=dict(label(299202,'Back',1740,375,160,84,32),touchable=True,
        alignment=37,beepEnable=True,iconPosition=pos(0,21,119,42),
        colorTab={'color0':0xffffff,'color1':0x838383,'color4':0x4c4e4d},
        picTab={'pic0':'dash/elements/btn_back_bg_n.png','pic1':'dash/elements/btn_back_bg_p.png'})
    return root
if __name__=='__main__':
    (ROOT/'assets').mkdir(exist_ok=True)
    data=screen();blob=encode(data);assert decode(blob)==data
    (ROOT/'assets/menu.ftu').write_bytes(blob)
    (ROOT/'assets/dashboard.dbp').write_bytes(package.compile_patch(json.loads((ROOT/'dashboard.json').read_text()),native_controls=True))
