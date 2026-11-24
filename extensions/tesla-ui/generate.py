"""Fixed cluster layout. Asset generation: uv run --with pillow --with resvg-py generate.py."""
import sys
from pathlib import Path
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT.parents[1]/'dashborne'))
from ui_codec import encode,decode
ASSETS='../../../mnt/extsd/Dashborne/extensions/tesla-ui/assets/'
def pos(x,y,w,h):return dict(left=x,top=y,width=w,height=h)
def label(id,text,x,y,w,h,size=24,color=0xe6e7e9,align=36):
    return dict(id=id,text=text,fontSize=size,alignment=align,colorTab={'color0':color},
                position=pos(x,y,w,h),touchable=False)
def picture(id,name,x,y,w,h):
    return dict(id=id,position=pos(x,y,w,h),backgroundPic=ASSETS+name+'.png',touchable=False)
def screen():
    s=dict(id=0,caption='Tesla UI',backgroundColor=0,position=pos(0,0,1920,480),
           resolution=dict(width=1920,height=480),visible=True,topmost=False,beepEnable=False,hideTimeout=-1)
    s['textview__energy_title']=label(599300,'ENERGY',185,75,430,32,21,0x8c9095)
    s['textview__power']=label(599301,'— kW',185,111,440,65,44)
    s['textview__history_caption']=label(599302,'Last 60 seconds',185,325,430,30,19,0x757a80)
    s['textview__zero']=dict(id=599303,position=pos(185,257,444,1),backgroundColor=0x272b30,touchable=False)
    for i in range(60):
        s[f'textview__history_{i:02}']=dict(id=599400+i,position=pos(185+i*7,257,5,1),
            backgroundColor=0xbfc4ca,visible=False,touchable=False)
    s['textview__nav']=label(599304,'',185,366,490,32,23)
    s['textview__nav_detail']=label(599305,'',185,400,490,26,20,0x8c9095)
    s['textview__car_park']=picture(599306,'park',720,103,500,300)
    s['textview__car_drive']=dict(picture(599307,'drive',865,145,230,260),visible=False)
    s['textview__state']=label(599308,'Connecting to vehicle',745,365,500,38,23,0x92969c,37)
    s['textview__speed_missing']=label(599309,'—',1355,137,300,148,126,align=37)
    for i in range(3):
        s[f'textview__speed_{i}']=dict(picture(599310+i,'digit-0',1391+i*76,140,76,148),visible=False)
    s['textview__unit']=label(599313,'',1365,285,280,32,23,0x92969c,37)
    s['textview__limit_0bg']=dict(picture(599314,'limit',1251,185,82,82),visible=False)
    s['textview__limit_1value']=label(599315,'',1252,198,80,54,31,0x1a1b1e,37)
    s['textview__battery_icon']=picture(599316,'battery',185,443,34,17)
    s['textview__battery_fill']=dict(id=599317,position=pos(188,446,20,11),backgroundColor=0x66ba6b,touchable=False,visible=False)
    s['textview__range']=label(599318,'—',232,431,175,40,23)
    s['textview__temperature']=label(599319,'—',424,431,145,40,23)
    s['textview__gear']=label(599320,'',1525,431,70,40,23,align=37)
    s['textview__clock']=label(599321,'',1615,431,175,40,23)
    s['textview__power_0line']=dict(id=599322,position=pos(735,431,710,3),backgroundColor=0x202328,touchable=False)
    s['textview__power_1fill']=dict(id=599323,position=pos(1090,431,1,3),backgroundColor=0xd9dce0,touchable=False,visible=False)
    for id,name,x in [(599324,'left',750),(599325,'right',1150)]:
        s['textview__turn_'+name]=dict(picture(id,name,x,28,38,28),visible=False)
    s['window__z_back']=dict(id=119300,position=pos(1826,385,80,80),
        button__back=dict(id=299300,caption='Back',position=pos(0,0,80,80),iconPosition=pos(8,8,64,64),
                         touchable=True,picTab={'pic0':ASSETS+'back.png','pic1':ASSETS+'back-pressed.png'}))
    return s
def assets():
    from PIL import Image,ImageDraw,ImageFont
    import resvg_py
    out=ROOT/'assets';out.mkdir(exist_ok=True)
    font=ImageFont.truetype(str(ROOT/'design/Inter.ttf'),126)
    font.set_variation_by_axes([28,400])
    for n in '0123456789':
        image=Image.new('RGBA',(76,148));draw=ImageDraw.Draw(image)
        draw.text((38,74),n,font=font,fill='#eeeeef',anchor='mm',stroke_width=0)
        image.save(out/f'digit-{n}.png')
    for name in ('battery','limit','left','right','back','back-pressed'):
        (out/f'{name}.png').write_bytes(resvg_py.svg_to_bytes(svg_string=(ROOT/'design'/f'{name}.svg').read_text()))
if __name__=='__main__':
    if '--assets' in sys.argv:assets()
    s=screen();blob=encode(s);assert decode(blob)==s
    (ROOT/'assets/cluster.ftu').write_bytes(blob)
