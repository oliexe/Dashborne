"""Generate the original display-control panel."""
import sys
from pathlib import Path
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT.parents[1]/'dashborne'))
from ui_codec import encode,decode

def pos(x,y,w,h): return dict(left=x,top=y,width=w,height=h)
def label(id,text,x,y,w,h,size=24):
    return dict(id=id,text=text,fontSize=size,alignment=36,colorTab={'color0':0xe8eaed},position=pos(x,y,w,h),touchable=False)
def button(id,text,x,y,w,h=54):
    return dict(label(id,text,x,y,w,h),touchable=True,alignment=37,backgroundColor=0x303338,
                colorTab={'color0':0xffffff,'color1':0xaaaaaa,'color2':0x70c6fd})
def screen():
    root=dict(id=0,caption='Better Display Controls',position=pos(0,0,1920,480),resolution=dict(width=1920,height=480),
              backgroundColor=0x08090a,visible=True,topmost=False,beepEnable=False,hideTimeout=-1)
    root['textview__title']=label(599519,'Display',100,20,310,52,32)
    root['button__enable']=button(299520,'Enable controls',90,95,300)
    root['button__mode']=button(299521,'Mode: Auto',90,163,300)
    root['textview__clock']=label(599521,'DASH clock: waiting for sync',90,235,310,70,20)
    root['button__save']=button(299522,'Saved',90,320,300)
    root['button__restore']=button(299523,'Restore stock',90,389,300)
    for i,title in enumerate(('Day','Night')):
        panel=dict(id=119530+i,position=pos(430+i*695,24,670,360),backgroundColor=0x181a1d)
        panel['textview__title']=label(599533+i*10,title,26,10,160,44,28)
        panel['textview__brightness']=label(599530+i*10,'Brightness',26,63,615,38)
        panel['textview__black']=label(599531+i*10,'Black level',26,175,615,38)
        for n,key,y,maximum in [(0,'brightness',105,254),(1,'black',217,100)]:
            panel['seekbar__'+key]=dict(id=919530+i*10+n,position=pos(26,y,615,54),max=maximum,
                backgroundPic='dash/settings/slider_bg.png',progressPic='dash/settings/slider_fg.png',
                thumb=dict(normalPic='dash/settings/slider_thumb.png',size=dict(width=30,height=30)))
        panel['textview__time']=label(599532+i*10,'Starts at',26,289,340,52)
        panel['button__minus']=button(299530+i*10,'-15m',400,289,110,52)
        panel['button__plus']=button(299531+i*10,'+15m',530,289,110,52)
        root['window__'+title.lower()]=panel
    root['textview__help']=label(599522,'Black level shifts image luminance; 50 is neutral. Brightness controls the backlight.',450,391,1280,30,20)
    root['textview__status']=label(599520,'Stock display controls',450,430,1280,32,20)
    root['button__back']=button(299529,'Back',1745,414,140,52)
    return root
if __name__=='__main__':
    s=screen();blob=encode(s);assert decode(blob)==s
    (ROOT/'assets/panel.ftu').write_bytes(blob)
