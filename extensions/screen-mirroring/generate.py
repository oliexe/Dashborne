"""Original standalone mirroring window, using the stock native video plane."""
import sys
from pathlib import Path
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT.parents[1]/'app/dashboard'))
from layout import encode,decode

def pos(x,y,w,h):return dict(left=x,top=y,width=w,height=h)
def label(id,text,x,y,w,h,size=24):
    return dict(id=id,text=text,fontSize=size,alignment=37,colorTab={'color0':0xffffff},
                position=pos(x,y,w,h),touchable=False)
def screen():
    root=dict(id=0,caption='Screen Mirroring',backgroundColor=0x000000,
              position=pos(0,0,1920,480),resolution=dict(width=1920,height=480),
              visible=True,topmost=False,beepEnable=False,hideTimeout=-1)
    root['videoview__mirror']=dict(id=699180,position=pos(16,80,1888,384),visible=False,touchable=False)
    wait=dict(id=119180,position=pos(16,80,1888,384),backgroundColor=0x000000)
    wait['textview__title']=label(599181,'Mirror your iPhone',250,90,1388,64,34)
    wait['textview__help']=label(599182,'Control Center  >  Screen Mirroring  >  your DASH',250,166,1388,50,26)
    wait['textview__return']=label(599183,'Back restores CarPlay / Android Auto.',250,230,1388,40,22)
    root['window__waiting']=wait
    root['textview__title']=label(599184,'Screen Mirroring',240,16,340,48,28)
    root['textview__status']=label(599180,'Starting receiver...',610,16,770,48,20)
    root['button__back']=dict(label(299180,'< Back',24,16,180,48),touchable=True,backgroundColor=0x222222)
    root['button__retry']=dict(label(299181,'Retry',1700,16,196,48),touchable=True,backgroundColor=0x222222)
    root['button__aspect']=dict(label(299182,'Aspect: Full width',1400,16,280,48),touchable=True,backgroundColor=0x222222)
    return root
if __name__=='__main__':
    data=screen();blob=encode(data);assert decode(blob)==data
    (ROOT/'assets').mkdir(exist_ok=True)
    (ROOT/'assets/viewer.ftu').write_bytes(blob)
