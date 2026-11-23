"""Draw original widget chrome and symbols from the theme tokens (Pillow)."""
from pathlib import Path
import json, math, re
from PIL import Image, ImageDraw, ImageFont
ROOT=Path(__file__).resolve().parent
PALETTES={'dash':dict(bg='#171717',card='#222222',line='#414141',text='#f0f0f0',muted='#a2a3a5',pressed='#333333'),
          'light':dict(bg='#f0f0f0',card='#ffffff',line='#dddddd',text='#171717',muted='#757575',pressed='#e0e0e0')}
BLUE='#2e78ff';GREEN='#25cb55'
FONT=ROOT/'assets/Inter-Regular.ttf'


def draw_asset(name,size):
    mode,tail=name.split('/',1);p=PALETTES[mode];w,h=size;scale=2
    im=Image.new('RGBA',(w*scale,h*scale));d=ImageDraw.Draw(im)
    def box(b,fill,r=0,outline=None,width=1):
        b=tuple(round(x*scale) for x in b)
        d.rounded_rectangle(b,radius=r*scale,fill=fill,outline=outline,width=width*scale)
    def line(points,fill,width=2):d.line([(int(x*scale),int(y*scale)) for x,y in points],fill=fill,width=width*scale,joint='curve')
    def poly(points,fill):d.polygon([(int(x*scale),int(y*scale)) for x,y in points],fill=fill)
    def text(x,y,t,size=24,color=None,anchor='mm'):
        d.text((int(x*scale),int(y*scale)),t,font=ImageFont.truetype(str(FONT),size*scale),fill=color or p['text'],anchor=anchor)
    stem=Path(tail).stem;pressed=stem.endswith('_p');selected=stem.endswith('_s') or '_s_nobg' in stem
    def card():
        transparent='_nobg' in stem
        box((1,1,w-2,h-2),None if transparent else p['pressed'] if pressed else p['card'],r=8,
            outline=BLUE if selected else None,width=2)
    if tail.startswith('btn_bg') or tail.startswith(('trip/trip_','projection/cpaa','clock/d2_','clock/a1_','tpms/tpms','gauges/1/g1_','gauges/2/g2_','gauges/4/g4_')) and not any(v in tail for v in ('_inds','_ind.','_pin','_thumb')):
        card()
        if 'valcomb' in stem:
            for y in (81,145):line([(24,y),(w-24,y)],p['line'],1)
        elif tail.startswith('trip/trip_'):
            line([(205,26),(205,h-26)],p['line'],1)
        elif tail.startswith('projection/'):
            if stem.endswith(('_as','_f')):box((2,2,w-3,h-3),None,r=8,outline=BLUE,width=2)
            else:
                box((w/2-19,h/2-46,w/2+19,h/2+20),None,r=6,outline=p['muted'],width=2)
                line([(w/2-7,h/2+13),(w/2+7,h/2+13)],p['muted'],2)
                text(w/2,h/2+65,'Phone projection',22,p['muted'])
        elif tail.startswith('clock/a1_'):
            cx,cy=w/2,h/2;r=min(w,h)*.39
            for i in range(60):
                a=i*math.pi/30;outer=r;inner=r-(14 if i%5==0 else 5)
                line([(cx+math.sin(a)*inner,cy-math.cos(a)*inner),(cx+math.sin(a)*outer,cy-math.cos(a)*outer)],p['text'] if i%5==0 else p['line'],2)
            for n in (3,6,9,12):
                a=n*math.pi/6;text(cx+math.sin(a)*(r-39),cy-math.cos(a)*(r-39),str(n),27)
        elif tail.startswith('tpms/'):
            small='1x1' in tail
            cx=w/2;cy=h/2;cw=38 if small else 108;ch=90 if small else 258
            box((cx-cw/2,cy-ch/2,cx+cw/2,cy+ch/2),p['line'],r=cw*.38,outline=p['muted'],width=2)
            box((cx-cw*.36,cy-ch*.27,cx+cw*.36,cy+ch*.23),p['bg'],r=cw*.18)
            for x in (cx-cw*.57,cx+cw*.43):
                for y in (cy-ch*.31,cy+ch*.23):box((x,y,x+cw*.14,y+ch*.16),p['muted'],r=2)
            if not small:text(cx,h-27,'TIRE PRESSURE',16,p['muted'])
        elif '/gauges/' in '/'+tail:
            # Quiet digital instrument; values and drive state remain native controls.
            line([(44,h-64),(w-44,h-64)],p['line'],1)
    elif tail.startswith('stats/media/btn_'):
        color=BLUE if pressed else p['text'];cx,cy=w/2,h/2
        if 'pause' in tail:
            for x in (w*.22,w*.61):box((x,h*.14,x+w*.17,h*.86),color,r=1)
        elif 'play' in tail:poly([(w*.2,h*.1),(w*.88,cy),(w*.2,h*.9)],color)
        else:
            backwards='rw_' in tail
            pts=[(w*.18,h*.1),(w*.75,cy),(w*.18,h*.9)]
            if backwards:pts=[(w-x,y) for x,y in pts]
            poly(pts,color);x=w*.12 if backwards else w*.82;box((x,h*.1,x+max(2,w*.08),h*.9),color)
    elif tail.startswith('gauges/gear/'):
        if stem.endswith('_line') and stem[0] in 'prnd':
            # Four-character state strip, one active gear per image.
            for i,c in enumerate('PRND'):text(w*(i+.5)/4,h/2,c,max(12,int(h*.65)),p['text'] if c.lower()==stem[0] else p['muted'])
        elif stem in ('gear_ind','gear_ind_nobg','gears_line'):
            for i,c in enumerate('PRND'):text(w*(i+.5)/4,h/2,c,max(12,int(h*.65)))
        elif 'bg' in stem:box((0,0,w-1,h-1),p['card'],r=4)
    elif tail.startswith('gauges/'):
        if 'bluering' in stem:
            # Autopilot state retains a blue outline, with no decorative glow.
            d.ellipse((4,4,w*scale-5,h*scale-5),outline=BLUE,width=2*scale)
        elif '/3/' in tail or 'arc' in stem:
            if 'pin' in stem:box((0,0,w-1,h-1),p['text'],r=2)
            else:d.ellipse((3,3,w*scale-4,h*scale-4),outline=BLUE,width=3*scale)
        # Speed rings/needles remain transparent: native digital speed is primary.
    elif tail.startswith('clock/') and 'pointer' in stem:
        line([(12,h/2),(w-3,h/2)],p['text'],max(2,min(h-2,5)))
        d.ellipse((8*scale,(h/2-4)*scale,16*scale,(h/2+4)*scale),fill=p['text'])
    elif any(t in tail for t in ('hbar','vbar','hor_bar')):
        color=p['line'] if 'bg' in stem else GREEN if 'green' in stem else p['muted'] if 'sub' in stem else BLUE
        if 'thumb' in stem:color=p['text']
        if w>h:box((0,max(0,h/2-2),w-1,min(h-1,h/2+2)),color,r=min(2,h/2))
        else:box((max(0,w/2-3),0,min(w-1,w/2+3),h-1),color,r=3)
    else:raise ValueError('Unclassified asset: '+tail)
    return im.resize((w,h),Image.Resampling.LANCZOS)


def generate():
    sizes=json.loads((ROOT/'resource-sizes.json').read_text())
    for name,size in sizes.items():
        out=ROOT/'assets/ui'/name;out.parent.mkdir(parents=True,exist_ok=True)
        draw_asset(name,size).save(out,optimize=True)
    print(f'Generated {len(sizes)} theme assets')


if __name__=='__main__':generate()
