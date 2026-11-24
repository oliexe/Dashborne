"""Draw original widget chrome and symbols from the theme tokens (Pillow)."""
from pathlib import Path
import json, math, re
from PIL import Image, ImageDraw, ImageFont
ROOT=Path(__file__).resolve().parent
PALETTES={'dash':dict(bg='#17181a',card='#242528',line='#3b3d41',text='#f4f4f4',muted='#a2a5a9',pressed='#3c3e42'),
          'light':dict(bg='#f1f2f3',card='#ffffff',line='#d7d9dc',text='#171a20',muted='#62666d',pressed='#e0e2e5')}
BLUE='#3e6ae1';GREEN='#39b96d'
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
    if tail.startswith('projection/'):
        d.rectangle((0,0,w*scale,h*scale),fill=p['bg'])
    stem=Path(tail).stem;pressed=stem.endswith('_p');selected=stem.endswith('_s') or '_s_nobg' in stem
    def card():
        transparent='_nobg' in stem
        box((1,1,w-2,h-2),None if transparent else p['pressed'] if pressed else p['card'],r=16,
            outline=BLUE if selected else None,width=2)
    if tail.startswith('preview/'):
        card()
        if stem.startswith('gauge'):
            if stem!='gaugeC_cfg':
                text(w/2,h*.44,'0',int(h*.30));text(w/2,h*.64,'km/h',25,p['muted'])
            line([(w*.12,h*.85),(w*.88,h*.85)],p['line'],1)
        elif stem in ('tripPreview','vals2x1_size'):
            # The native editor draws the changing values over these backgrounds.
            for y in (78,148):line([(w*.47 if stem=='tripPreview' else 24,y),(w-24,y)],p['line'],1)
        elif stem.startswith(('trip','vals')):
            for i,(label,value) in enumerate([('Distance','0.0 km'),('Time','0 min'),('Consumption','0 Wh/km')]):
                y=43+i*67;text(24,y,label,22,p['muted'],'lm');text(w-24,y,value,27,p['text'],'rm')
                if i<2:line([(24,y+32),(w-24,y+32)],p['line'],1)
        elif stem.startswith('media'):
            text(w/2,h*.3,'Media',25);text(w/2,h*.5,'Connect your phone',17,p['muted'])
            cy=h*.76
            for cx in (w*.25,w*.5,w*.75):
                poly([(cx-6,cy-9),(cx+8,cy),(cx-6,cy+9)],p['text'])
        elif stem.startswith('nav'):
            line([(w*.2,h*.4),(w*.2,h*.25),(w*.1,h*.25)],p['text'],3)
            poly([(w*.1,h*.25),(w*.14,h*.21),(w*.14,h*.29)],p['text'])
            text(w*.62,h*.3,'200 m',28);text(w/2,h*.6,'Next turn',22,p['muted'])
        elif stem.startswith('pressure'):
            for xx in (.24,.76):
                for yy in (.25,.75):text(w*xx,h*yy,'2.8',24)
            box((w*.43,h*.22,w*.57,h*.78),p['line'],r=14,outline=p['muted'],width=2)
        elif 'clock' in stem.lower():
            text(w/2,h*.48,'12:45',min(80,int(w*.15)));text(w/2,h*.73,'Tuesday',22,p['muted'])
        elif stem.startswith('warn'):
            text(w/2,h*.42,'!',48);text(w/2,h*.68,'Vehicle status',21,p['muted'])
        elif stem.startswith('vid'):
            ww=min(110,w*.3);hh=min(155,h*.5);cx=w/2;cy=h/2
            box((cx-ww/2,cy-hh/2,cx+ww/2,cy+hh/2),None,r=8,outline=p['muted'],width=2)
            text(w/2,h*.82,stem[3:6],22,p['muted'])
    elif tail.startswith(('elements/','ime/','dialog/')):
        color=BLUE if pressed else p['text'];cx=w/2;cy=h/2
        if tail.startswith('dialog/') and stem in ('question_ico','info_ico'):
            # Native dialogs sometimes select the dark icon in either theme.
            color='#7d8188';radius=min(w,h)*.42
            d.ellipse(((cx-radius)*scale,(cy-radius)*scale,(cx+radius)*scale,(cy+radius)*scale),outline=color,width=3*scale)
            text(cx,cy,'?' if stem=='question_ico' else '!',44,color)
        elif stem.startswith(('btn_ok','btn_dlg_confirm')):
            box((1,1,w-2,h-2),p['line'] if stem.endswith('_i') else '#2854bd' if pressed else BLUE,r=6)
        elif stem.startswith(('arrow_','btn_back')):
            if stem.startswith('btn_back'):color='#85898f'
            direction='left' if 'back' in stem or 'left' in stem else 'right' if 'right' in stem else 'up' if 'up' in stem else 'down'
            cx=18 if 'back' in stem else w/2;r=min(12,h*.3,w*.38)
            points=[(cx+r/2,cy-r),(cx-r/2,cy),(cx+r/2,cy+r)]
            if direction=='right':points=[(2*cx-x,y) for x,y in points]
            elif direction in ('up','down'):
                points=[(cx-r,cy+r/2),(cx,cy-r/2),(cx+r,cy+r/2)]
                if direction=='down':points=[(x,2*cy-y) for x,y in points]
            line(points,color,3)
        elif stem.startswith(('btn_add_','btn_rem_','btn_dup_','btn_edit_')):
            box((3,3,w-4,h-4),p['pressed'] if pressed else p['card'],r=min(w,h)/2)
            r=min(w,h)*.23
            if 'edit' in stem:line([(cx-r,cy+r),(cx+r,cy-r)],color,5)
            elif 'dup' in stem:
                box((cx-r,cy-r,cx+r*.65,cy+r*.65),None,r=2,outline=color,width=2)
                line([(cx-r*.5,cy+r),(cx+r,cy+r),(cx+r,cy-r*.5)],color,2)
            else:
                line([(cx-r,cy),(cx+r,cy)],color,3)
                if 'add' in stem:line([(cx,cy-r),(cx,cy+r)],color,3)
        elif stem.startswith(('tab_stats','tab_actions','tab_special','btn_page_edit')):
            label='Stats' if 'stats' in stem else 'Actions' if 'actions' in stem else 'Special' if 'special' in stem else 'Page Edit'
            tile=Image.new('RGBA',(h*scale,w*scale));td=ImageDraw.Draw(tile)
            td.text((h*scale/2,w*scale/2),label,font=ImageFont.truetype(str(FONT),26*scale),fill=p['text'] if stem.endswith('_on') or 'page_edit' in stem else p['muted'],anchor='mm')
            im.alpha_composite(tile.rotate(90,expand=True))
        elif stem.startswith('submenu_bar'):
            box((0,0,w-1,h-1),p['text'] if stem.endswith('_p') else p['line'])
        elif stem=='tab_bot_on':box((0,0,w-1,h-1),p['text'])
        elif stem in ('tab_bar_i','page_indicator_bg'):pass
        elif stem=='tab_bar_bg':line([(0,h-2),(w,h-2)],p['line'],1)
        elif stem=='keyboard_bg':box((0,0,w-1,h-1),p['bg'],r=10)
        elif stem.startswith('pass_'):
            d.ellipse((2*scale,4*scale,(w-2)*scale,(h-4)*scale),outline=color,width=2*scale)
            d.ellipse(((cx-3)*scale,(cy-3)*scale,(cx+3)*scale,(cy+3)*scale),fill=color)
            if 'hidden' in stem:line([(3,h-2),(w-3,2)],color,2)
        else:
            box((1,1,w-2,h-2),p['pressed'] if pressed else p['card'],r=6,outline=p['line'],width=1)
            if stem.startswith('backsp'):
                line([(cx-13,cy),(cx-5,cy-9),(cx+15,cy-9),(cx+15,cy+9),(cx-5,cy+9),(cx-13,cy)],color,2)
                line([(cx,cy-4),(cx+8,cy+4)],color,2);line([(cx,cy+4),(cx+8,cy-4)],color,2)
            elif stem.startswith('enter'):
                line([(cx+12,cy-8),(cx+12,cy+5),(cx-12,cy+5),(cx-5,cy-2)],color,2)
                line([(cx-12,cy+5),(cx-5,cy+12)],color,2)
            elif stem.startswith('shift'):
                poly([(cx,cy-12),(cx+14,cy),(cx+6,cy),(cx+6,cy+12),(cx-6,cy+12),(cx-6,cy),(cx-14,cy)],color)
            elif stem.startswith('symbols'):text(cx,cy,'123',22,color)
    elif tail.startswith('settings/'):
        selected=stem.endswith(('_s','_on'));pressed=stem.endswith('_p')
        if stem in ('btn_toggle_on','btn_toggle_off','auto_on','auto_off'):
            on=stem.endswith('_on');hh=min(h-2,32);ww=min(w-2,64);x=1;y=(h-hh)/2
            box((x,y,x+ww,y+hh),BLUE if on else p['line'],r=hh/2)
            cx=x+ww-hh/2 if on else x+hh/2
            d.ellipse(((cx-hh*.38)*scale,(y+hh*.12)*scale,(cx+hh*.38)*scale,(y+hh*.88)*scale),fill='#ffffff')
        elif stem.startswith(('leftcb','midcb','rightcb')):
            fill=p['pressed'] if mode=='dash' else '#ffffff' if selected else p['bg']
            if not selected:fill=p['bg']
            box((1,1,w-2,h-2),fill,r=6,outline=p['line'],width=1)
        elif stem.startswith('listitem'):
            if stem.endswith('_s'):box((1,1,w-2,h-2),p['pressed'] if mode=='dash' else '#ffffff',r=8)
        elif stem in ('slider_bg','dwbar_bg'):
            box((1,h/2-3,w-2,h/2+3),p['line'],r=3)
        elif stem in ('slider_fg','dwbar_val'):
            box((1,h/2-3,w-2,h/2+3),p['text'],r=3)
        elif 'thumb' in stem:
            rr=min(w,h)*.34;cx=w/2;cy=h/2
            d.ellipse(((cx-rr)*scale,(cy-rr)*scale,(cx+rr)*scale,(cy+rr)*scale),fill=p['text'])
        elif stem.startswith('step_'):
            rr=min(w,h)*.4;cx=w/2;cy=h/2
            d.ellipse(((cx-rr)*scale,(cy-rr)*scale,(cx+rr)*scale,(cy+rr)*scale),outline=p['muted'],width=2*scale)
            if stem[-1].isdigit():text(cx,cy,stem[-1],int(h*.45))
            elif stem=='step_complete':line([(cx-rr/2,cy),(cx,cy+rr/2),(cx+rr/2,cy-rr/2)],p['text'],2)
        elif stem in ('restart','export','export_p','settings_n','settings_p'):
            # Original geometric glyphs, with the same native hit area.
            cx=w/2;cy=h/2;r=min(w,h)*.28;color=BLUE if pressed else p['text']
            if stem=='restart':
                d.arc(((cx-r)*scale,(cy-r)*scale,(cx+r)*scale,(cy+r)*scale),25,310,fill=color,width=3*scale)
                poly([(cx+r*.65,cy-r),(cx+r*1.1,cy-r*.15),(cx+r*.05,cy-r*.3)],color)
            elif stem.startswith('export'):
                line([(cx-r,cy),(cx-r,cy+r),(cx+r,cy+r),(cx+r,cy)],color,2)
                line([(cx,cy+r/3),(cx,cy-r),(cx-r/2,cy-r/2)],color,2)
                line([(cx,cy-r),(cx+r/2,cy-r/2)],color,2)
            else:
                for yy in (cy-r,cy,cy+r):line([(cx-r,yy),(cx+r,yy)],color,2)
        elif stem in ('line25x5',):box((0,0,w-1,h-1),p['line'])
        elif stem in ('header_bg','halfheader_bg','large_bg','pic_ctrls_bg','tz_bg','unpair_bg','btdev_s'):
            box((1,1,w-2,h-2),p['card'],r=min(10,h/4))
        elif stem=='list_bg':pass
        else:
            box((1,1,w-2,h-2),p['pressed'] if pressed else p['card'],r=min(8,h/4),outline=p['line'] if 'btn' in stem else None)
    elif tail.startswith('btn_bg') or tail.startswith(('trip/trip_','projection/cpaa','clock/d2_','clock/a1_','tpms/tpms','gauges/1/g1_','gauges/2/g2_','gauges/4/g4_')) and not any(v in tail for v in ('_inds','_ind.','_pin','_thumb')):
        card()
        if 'valcomb' in stem:
            for y in (81,145):line([(24,y),(w-24,y)],p['line'],1)
        elif tail.startswith('trip/trip_'):
            line([(205,26),(205,h-26)],p['line'],1)
        elif tail.startswith('projection/'):
            if stem.endswith(('_as','_f')):box((2,2,w-3,h-3),None,r=16,outline=BLUE,width=2)
            else:
                box((w/2-19,h/2-33,w/2+19,h/2+33),None,r=6,outline=p['muted'],width=2)
                line([(w/2-7,h/2+26),(w/2+7,h/2+26)],p['muted'],2)
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
            else:
                bounds=(3,3,w*scale-4,h*scale-4)
                for begin,end,color in [(135,267,GREEN),(273,405,p['text']),(47,133,BLUE)]:
                    d.arc(bounds,begin,end,fill=color,width=3*scale)
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
    routes=[n for n in sizes if ('/elements/' in n or '/preview/' in n or n.endswith(('/question_ico.png','/info_ico.png'))) and '/submenu_bar' not in n and '/ps_btn' not in n]
    if len(routes)>124:raise ValueError('Image route count exceeds the tested adapter')
    (ROOT/'image_routes.inc').write_text('/* Native image names eligible for theme-aware routing. */\nstatic const char *image_assets[]={\n'+''.join(json.dumps(n)+',\n' for n in sorted(routes))+'};\n')
    print(f'Generated {len(sizes)} theme assets')


if __name__=='__main__':generate()
