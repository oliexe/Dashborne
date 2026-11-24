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
    s['textview__energy_title']=label(599300,'Power',185,82,414,30,23,0xb6b8bc,37)
    s['textview__power']=label(599301,'— kW',185,328,414,32,24,align=37)
    s['textview__history_caption']=label(599302,'Past 60 seconds',185,360,414,26,18,0x757a80,37)
    s['textview__zero']=dict(id=599303,position=pos(185,262,414,1),backgroundColor=0x303236,touchable=False)
    for i,(value,y) in enumerate((('100',132),('0',252),('−50',312))):
        s[f'textview__axis_{i}']=label(599330+i,value,612,y,54,20,15,0x65686c)
    for i in range(414):
        s[f'textview__graph_fill_{i:03}']=dict(picture(600000+i,'energy-fill',185+i,262,1,1),visible=False)
        s[f'textview__graph_trace_{i:03}']=dict(id=601000+i,position=pos(185+i,262,1,2),
            backgroundColor=0xeaa34b,visible=False,touchable=False)
    for i in range(35):
        s[f'textview__graph_mean_{i:02}']=dict(id=602000+i,position=pos(185+i*12,262,6,1),
            backgroundColor=0x85817a,visible=False,touchable=False)
    s['textview__nav']=label(599304,'',185,395,490,26,21)
    s['textview__preview_status']=dict(label(599333,'',185,25,500,38,20,0xeaa34b),visible=False)
    s['window__z_preview']=dict(id=119301,position=pos(1710,24,180,48),
        button__preview=dict(label(299301,'Preview',0,0,180,48,20,0xc8ccd1,37),
            touchable=True,backgroundColor=0x17191d,bgColorTab={'color1':0x30343b}))
    # Alphabetical control order keeps the road, markings and side zones behind the car.
    s['textview__car_00_road']=dict(picture(603000,'road',680,55,600,360),visible=False)
    for i in range(12):
        s[f'textview__car_01_mark_{i:02}']=dict(picture(603004+i,'road-dash-'+('left' if i<6 else 'right'),
            980,100,3,5),visible=False)
    s['textview__car_02_fog']=dict(picture(603001,'road-fog',680,55,600,360),visible=False)
    for i,side in enumerate(('left','right')):
        s['textview__car_03_zone_'+side]=dict(picture(603002+i,'road-'+side+'-turn',710 if i==0 else 1038,158,212,189),visible=False)
    s['textview__car_park']=picture(599306,'park',720,103,500,300)
    s['textview__car_drive']=dict(picture(599307,'drive',865,125,230,260),visible=False)
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
    for i,rect in enumerate(((1824,383,84,2),(1824,467,84,2),(1824,385,2,82),(1906,385,2,82))):
        s[f'textview__focus_{i}']=dict(id=599326+i,position=pos(*rect),backgroundColor=0xd9dce0,
            visible=False,touchable=False)
    s['window__z_back']=dict(id=119300,position=pos(1826,385,80,80),
        button__back=dict(id=299300,caption='Back',position=pos(0,0,80,80),iconPosition=pos(8,8,64,64),
                         touchable=True,picTab={'pic0':ASSETS+'back.png','pic1':ASSETS+'back-pressed.png'}))
    return s
def assets():
    from PIL import Image,ImageDraw,ImageFont
    import resvg_py
    from road_assets import generate
    out=ROOT/'assets';out.mkdir(exist_ok=True)
    generate(out)
    font=ImageFont.truetype(str(ROOT/'design/Inter.ttf'),126)
    font.set_variation_by_axes([28,400])
    for n in '0123456789':
        image=Image.new('RGBA',(76,148));draw=ImageDraw.Draw(image)
        draw.text((38,74),n,font=font,fill='#eeeeef',anchor='mm',stroke_width=0)
        image.save(out/f'digit-{n}.png')
    for name,color,reverse in (('energy-fill',(200,126,43),False),('regen-fill',(89,174,115),True)):
        image=Image.new('RGBA',(1,180))
        for y in range(180):
            strength=(y/179 if reverse else 1-y/179)
            image.putpixel((0,y),(*color,round(60*strength)))
        image.save(out/f'{name}.png')
    for name in ('battery','limit','left','right','back','back-pressed'):
        (out/f'{name}.png').write_bytes(resvg_py.svg_to_bytes(svg_string=(ROOT/'design'/f'{name}.svg').read_text()))
if __name__=='__main__':
    if '--assets' in sys.argv:assets()
    s=screen();blob=encode(s);assert decode(blob)==s
    (ROOT/'assets/cluster.ftu').write_bytes(blob)
