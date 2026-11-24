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
    s['textview__energy_title']=label(599300,'Power',225,82,414,30,23,0xb6b8bc,37)
    s['textview__power']=label(599301,'— kW',225,328,414,32,24,align=37)
    s['textview__history_caption']=label(599302,'Past 60 seconds',225,360,414,26,18,0x757a80,37)
    s['textview__zero']=dict(id=599303,position=pos(225,262,414,1),backgroundColor=0x303236,touchable=False)
    for i,(value,y) in enumerate((('100',132),('0',252),('−50',312))):
        s[f'textview__history_axis_{i}']=label(599330+i,value,652,y,54,20,15,0x65686c)
    for i in range(414):
        s[f'textview__graph_fill_{i:03}']=dict(picture(600000+i,'energy-fill',225+i,262,1,1),visible=False)
        s[f'textview__graph_trace_{i:03}']=dict(id=601000+i,position=pos(225+i,262,1,2),
            backgroundColor=0xeaa34b,visible=False,touchable=False)
    for i in range(35):
        s[f'textview__graph_mean_{i:02}']=dict(id=602000+i,position=pos(225+i*12,262,6,1),
            backgroundColor=0x85817a,visible=False,touchable=False)
    s['textview__nav']=label(599304,'',225,395,490,26,21)
    s['textview__preview_status']=dict(label(599333,'',225,25,500,38,20,0xeaa34b),visible=False)
    s['window__z_preview']=dict(id=119301,position=pos(1710,24,180,48),
        button__preview=dict(label(299301,'Preview',0,0,180,48,20,0xc8ccd1,37),
            touchable=True,backgroundColor=0x17191d,bgColorTab={'color1':0x30343b}))
    # Alphabetical control order keeps the road, markings and side zones behind the car.
    s['textview__car_00_road']=dict(picture(603000,'road',680,55,600,360),visible=False)
    s['textview__car_02_fog']=dict(picture(603001,'road-fog',680,55,600,360),visible=False)
    for i,side in enumerate(('left','right')):
        s['textview__car_03_zone_'+side]=dict(picture(603002+i,'road-'+side+'-turn',710 if i==0 else 1038,158,212,189),visible=False)
    s['textview__car_park']=picture(599306,'park',680,55,600,360)
    s['textview__car_drive']=dict(picture(599307,'drive',680,55,600,360),visible=False)
    s['textview__state']=label(599308,'Connecting to vehicle',745,395,500,28,18,0x92969c,37)
    s['textview__speed_missing']=label(599309,'—',1240,177,250,130,108,align=37)
    for i in range(3):
        s[f'textview__speed_{i}']=dict(picture(599310+i,'digit-0',1245+i*80,177,80,130),visible=False)
    s['textview__unit']=label(599313,'',1240,300,250,28,18,0x92969c,37)
    s['textview__limit_0bg']=dict(picture(599314,'limit',1122,35,50,50),visible=False)
    s['textview__limit_1value']=label(599315,'',1123,43,48,34,21,0x1a1b1e,37)
    s['textview__battery_icon']=picture(599316,'battery',469,437,34,17)
    s['textview__battery_fill']=dict(id=599317,position=pos(472,440,20,11),backgroundColor=0x66ba6b,touchable=False,visible=False)
    s['textview__range']=label(599318,'—',511,425,130,40,21)
    s['textview__temperature']=label(599319,'—',645,425,125,40,21)
    s['textview__gear']=label(599320,'',1248,425,110,40,21,align=37)
    s['textview__clock']=label(599321,'',1370,425,175,40,21)
    s['textview__power_0line']=dict(id=599322,position=pos(730,469,450,3),backgroundColor=0x202328,touchable=False)
    s['textview__power_1fill']=dict(id=599323,position=pos(955,469,1,3),backgroundColor=0xd9dce0,touchable=False,visible=False)
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
    font=ImageFont.truetype(str(ROOT/'design/Montserrat.ttf'),108)
    font.set_variation_by_axes([500])
    (ROOT/'digit_metrics.h').write_text('/* Generated by generate.py. */\nstatic const int digit_advances[]={'+','.join(str(round(font.getlength(n))) for n in '0123456789')+'};\n')
    for n in '0123456789':
        image=Image.new('RGBA',(80,130));draw=ImageDraw.Draw(image)
        draw.text((4,65),n,font=font,fill='#eeeeef',anchor='lm',stroke_width=0)
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
