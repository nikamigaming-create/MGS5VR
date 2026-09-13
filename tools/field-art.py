"""Code-native layouts reusing the unmodified original field-card artwork.

The bitmap is embedded once per SVG for GitHub/offline portability. Viewports
reuse its illustrations; all current instructions remain selectable SVG text.
"""
import base64,html,pathlib

ROOT=pathlib.Path(__file__).resolve().parents[1]
PAPER='#f2efe5'
INK='#252625'
RED='#af2624'

def begin(width,height,title):
    sheet=base64.b64encode((ROOT/'docs/images/controls.png').read_bytes()).decode('ascii')
    return [f'<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" width="{width}" height="{height}" viewBox="0 0 {width} {height}" role="img" aria-labelledby="title"><title id="title">{html.escape(title)}</title>',
        f'<defs><image id="original" width="1086" height="1448" xlink:href="data:image/png;base64,{sheet}"/></defs>',
        f'<rect width="{width}" height="{height}" fill="{PAPER}"/><g font-family="Arial, Helvetica, sans-serif">']

def crop(parts,x,y,w,h,source):
    parts.append(f'<svg x="{x}" y="{y}" width="{w}" height="{h}" viewBox="{" ".join(map(str,source))}" overflow="hidden"><use xlink:href="#original"/></svg>')

def text(parts,x,y,value,size=24,weight=400,color=INK):
    parts.append(f'<text x="{x}" y="{y}" fill="{color}" font-size="{size}" font-weight="{weight}">{html.escape(value)}</text>')

def rect(parts,x,y,w,h,color):
    parts.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" fill="{color}"/>')

def header(parts):
    crop(parts,0,0,1440,288,(0,0,1086,217.2))
    rect(parts,0,187,797,101,PAPER)
    text(parts,36,235,'NATIVE VR / FIELD MANUAL',33,900)
    text(parts,38,272,'THE PHANTOM PAIN  /  WINDOWS  /  OPENXR',20,700)

def save(parts,name):
    (ROOT/'docs/images'/name).write_text(''.join(parts)+'</g></svg>',encoding='utf-8')

if __name__=='__main__':
    p=begin(1440,302,'MGS5VR native VR field manual, with the original Snake portrait')
    header(p);rect(p,30,293,1380,3,RED);save(p,'field-header.svg')

    p=begin(1440,270,'Original Snake support-hand and grenade illustrations, with current grip instructions')
    crop(p,0,0,1440,270,(0,1215,1086,203.625))
    rect(p,57,16,267,168,PAPER)
    text(p,58,47,'SUPPORT HAND',27,900)
    text(p,58,91,'Hold left grip at the',22)
    text(p,58,123,"weapon’s support socket.",22)
    text(p,58,155,'Release grip to let go.',22)
    save(p,'field-gear.svg')

    p=begin(1440,1230,'Current default Quest Touch controls; detailed accessible bindings are in CONTROLS.md')
    rect(p,30,24,12,80,RED);text(p,65,65,'FIELD CONTROLS',42,900)
    text(p,65,99,'DEFAULT VR MODE / QUEST TOUCH / ACTION TYPE',21,700)
    rect(p,30,130,1380,48,INK)
    text(p,56,162,'LEFT HAND',25,800,PAPER);text(p,754,162,'RIGHT HAND',25,800,PAPER)
    # Keep the old card's decorative callout lines inside separate reference
    # panels. They must not appear to connect to today's different bindings.
    for x in (468,1172):
        p.append(f'<rect x="{x}" y="202" width="215" height="470" fill="{PAPER}" stroke="#b9b8ad"/>')
    crop(p,486,218,179,390,(165,343,238,509))
    crop(p,1190,218,179,390,(717,341,234,510))
    text(p,486,647,'TOUCH / LEFT',18,700)
    text(p,1190,647,'TOUCH / RIGHT',18,700)
    left=[('STICK','Move'),('STICK CLICK','Sprint'),('GRIP','Support the held weapon'),('TRIGGER','Open wrist equipment'),('X / Y','Commands hold / context'),('L GRIP + B / Y','Reload / binoculars')]
    right=[('STICK LEFT / RIGHT','Turn'),('STICK UP','Scope zoom while ready'),('STICK CLICK','Dive'),('GRIP / TRIGGER','Ready / fire or throw'),('A','Tap: crouch / hold: prone'),('B','Pickup / carry')]
    for x,rows in [(56,left),(754,right)]:
        for i,(label,action) in enumerate(rows):
            y=245+i*78;text(p,x,y,label,23,800,RED);text(p,x,y+30,action,21)
    rect(p,30,723,1380,3,RED)
    sections=[(52,'01 / WRIST PICKER',['Hold left trigger. Wait for the bar.','Flick right stick to a category.','Center; flick to an item. Release LT.']),
              (523,'02 / BINOCULARS',['Hold left grip + Y to equip; B stows.','Raise the device to your eye.','Left click: 2x / 4x; right stick up runs.']),
              (994,'03 / CONTEXT',['B: pickup / carry; left grip + B: reload.','Y: native interaction or Fulton.','Right grip + R3: quick-switch ready weapon.'])]
    for x,title,lines in sections:
        text(p,x,778,title,25,900)
        for i,line in enumerate(lines):text(p,x,820+i*34,line,21)
    crop(p,32,943,302,214,(245,1218,267,190))
    text(p,380,982,'YOUR CONTROLS. YOUR CALL.',32,900)
    text(p,380,1026,'Open Edit-Controls.cmd or the launcher controls editor.',23)
    text(p,380,1061,'Save, then release inputs and center sticks for two seconds.',23)
    text(p,380,1096,'Bindings apply live. Your custom file takes priority over this card.',23)
    text(p,380,1143,'Menu + A hold: native-button mode. Left grip + Menu: recenter.',21,700)
    text(p,34,1201,'Full bindings, mounted controls and remaining limitations: docs/CONTROLS.md',20)
    save(p,'controls-quick.svg')
    print('Reused original artwork in field-header.svg, field-gear.svg and controls-quick.svg')
