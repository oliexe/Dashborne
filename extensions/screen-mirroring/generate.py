"""Original standalone mirroring window, using the stock native video plane."""
import sys
from pathlib import Path
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT.parents[1]/'dashborne'))
from ui_codec import encode,decode

def pos(x,y,w,h):return dict(left=x,top=y,width=w,height=h)
def label(id,text,x,y,w,h,size=24):
    return dict(id=id,text=text,fontSize=size,alignment=37,colorTab={'color0':0xffffff},
                position=pos(x,y,w,h),touchable=False)
ASSETS='../../../mnt/extsd/Dashborne/extensions/screen-mirroring/assets/'
def control(id,name,x,y,container):
    return dict(id=container,position=pos(x,y,80,80),backgroundColor=0x000000,
                **{'button__icon':dict(id=id,caption=name,position=pos(0,0,80,80),
                    iconPosition=pos(8,8,64,64),touchable=True,
                    picTab={'pic0':ASSETS+name+'.png','pic1':ASSETS+name+'-pressed.png'})})
def screen():
    root=dict(id=0,caption='Screen Mirroring',backgroundColor=0x000000,
              position=pos(0,0,1920,480),resolution=dict(width=1920,height=480),
              visible=True,topmost=False,beepEnable=False,hideTimeout=-1)
    root['videoview__mirror']=dict(id=699180,position=pos(0,0,1920,480),visible=False,touchable=False)
    wait=dict(id=119180,position=pos(0,0,1920,480),backgroundColor=0x000000)
    wait['textview__title']=label(599181,'Mirror your iPhone',300,125,1320,64,34)
    wait['textview__help']=label(599182,'Control Center  >  Screen Mirroring  >  your DASH',300,205,1320,50,26)
    wait['textview__status']=label(599180,'Starting receiver...',300,270,1320,48,22)
    root['window__waiting']=wait
    chooser=dict(id=119186,position=pos(350,40,1220,360),backgroundColor=0x101010)
    chooser['textview__title']=label(599187,'Screen Mirroring',40,20,1140,56,34)
    for id,name,y in [(299183,'AirPlay',110),(299184,'Miracast (experimental)',210)]:
        chooser['button__'+name.split()[0]]=dict(label(id,name,40,y,480,80,28),
            touchable=True,selected=id==299183,
            picTab={'pic0':'dash/settings/listitem_bg_n.png',
                    'pic2':'dash/settings/listitem_bg_s.png'})
    chooser['textview__status']=label(599186,'Mirror your iPhone using AirPlay.',560,110,620,90,24)
    chooser['button__start']=dict(label(299185,'Start mirroring',670,230,400,80,28),
        touchable=True,picTab={'pic0':'dash/settings/btn_con_n.png',
                              'pic1':'dash/settings/btn_con_p.png'})
    root['window__selector']=chooser
    # Native children are created in member-name order. Small overlay windows
    # sort after the video/standby view, keeping controls visible above both.
    root['window__z_aspect']=control(299182,'aspect',130,14,119181)
    root['window__z_back']=control(299180,'back',1826,385,119182)
    root['window__z_retry']=control(299181,'retry',1826,14,119183)
    root['window__z_hint']=dict(id=119184,position=pos(800,400,320,56),
        backgroundColor=0x17191d,visible=False,touchable=False,
        textview__ratio=label(599185,'Full width',0,0,320,56,24))
    return root
if __name__=='__main__':
    data=screen();blob=encode(data);assert decode(blob)==data
    (ROOT/'assets').mkdir(exist_ok=True)
    (ROOT/'assets/viewer.ftu').write_bytes(blob)
