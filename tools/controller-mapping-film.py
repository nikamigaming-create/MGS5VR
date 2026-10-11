"""Build an explained launcher walkthrough from actual UI regression captures.

Launcher/controller illustrations are instructional material, not gameplay
acceptance. An optional reviewed compositor clip keeps its distinct source
label. All labels and the complete control index use the native binding export.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
import subprocess
import wave
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
SIZE = (1920, 1080)
INK = '#202923'
PAPER = '#f1eddf'
RED = '#e04c37'
AMBER = '#eab956'
MUTED = '#a9b9ad'
FONTS = Path('C:/Windows/Fonts')


def font(size, bold=False):
    return ImageFont.truetype(str(FONTS / ('segoeuib.ttf' if bold else 'segoeui.ttf')), size)


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def words(draw, text, x, y, width, size=25, color=INK, bold=False, spacing=1.38):
    face = font(size, bold)
    line = ''
    for word in text.split():
        candidate = (line + ' ' + word).strip()
        if line and draw.textlength(candidate, font=face) > width:
            draw.text((x, y), line, font=face, fill=color)
            y += int(size * spacing)
            line = word
        else:
            line = candidate
    if line:
        draw.text((x, y), line, font=face, fill=color)
        y += int(size * spacing)
    return y


def label(binding):
    if not binding:
        return 'Unbound'
    names = {'a': 'A', 'b': 'B', 'x': 'X', 'y': 'Y', 'menu': 'L Menu'}
    inputs = [names.get(t, t.replace('left_', 'L ').replace('right_', 'R ').replace('_', ' ')) for t in binding['inputs']]
    prefix = {'level': 'Hold', 'press': 'Press', 'release': 'Release', 'tap': 'Tap', 'hold': 'Long hold'}[binding['gesture']]
    threshold = f" ({binding['milliseconds']} ms)" if binding['gesture'] in ('tap', 'hold') else ''
    return prefix + ' ' + ' + '.join(inputs) + threshold


def scene(shot, title, narration, bullets, *, selector=None, second=None, action=None, mode=None, kind='launcher'):
    return dict(shot=shot, title=title, narration=narration, bullets=bullets,
                selector=selector, second=second, action=action, mode=mode, kind=kind)


def make_script(bindings, clip, editor_clip):
    scenes = [
        scene('02-reload', 'Map your controllers',
              'This walkthrough shows the actual MGS5VR launcher. You will find a control, choose physical inputs, add a chord or an alternative, save it, and check the result. The examples use a copy of the saved layout. Your own bindings can be different.',
              ['Actual launcher and settings validator', 'Examples leave your saved controls unchanged', 'Every supported action is in Controller bindings'], selector='#action-select'),
        scene('01-home', 'Open Controller bindings',
              'Open the launcher for your installed game. Choose Controller bindings on the left. Game modes explains which controls own input in each situation. The editor writes the controls file beside your game, so changing the repository template does not remap your installed copy.',
              ['Select your installed MGSV game', 'Open Controller bindings', 'Game modes explains each context'], selector='nav [data-tab="controls"]'),
        scene('03-search', 'Find the action you want',
              'Type a name such as reload or iDroid in Find a control, then choose the exact action below. Read Used in before changing it. The same button can perform different actions in different modes. The editor includes optional actions even when they are unbound.',
              ['Search by action or device', 'Choose the exact action', 'Read its context before editing'], selector='#action-select'),
        scene('tour-gameplay-reload', 'Build a deliberate chord',
              'A chord uses all of its selected inputs together. In this saved layout, reload is a short tap of left grip and B. Keep the grip down, tap B, then release. The longer chord consumes the simpler pickup action. Red on the controller shows a press or squeeze.',
              ['Selected inputs combine with +', 'Use the complete chord', 'Release before repeating'], selector='#tour-binding', action='gameplay.reload'),
        scene('04-ready', 'Add an alternative',
              'Alternative means either complete option can trigger the action. It does not add another button to the first chord. Here weapon ready keeps its original right grip binding. We add a second option using right trigger touch, so either input can ready the gun.',
              ['+ inside one option means together', 'Alternative means either option works', 'Keep a familiar binding while trying another'], second='05-touch-alternative'),
        scene('05-touch-alternative', 'Pick touch instead of squeeze',
              'Choose Light touch in Pick a surface as, then pick the trigger on the three dimensional controller or its named input button. Remove the placeholder A from the new option and choose While held. Check the binding readout before saving. Trigger touch senses contact before you squeeze.',
              ['Pick a surface as → Light touch', 'Remove the placeholder input', 'Use While held for a sustained action'], selector='[data-row="1"][data-token="right_trigger_touch"]'),
        scene('touch-2', 'Touch and press are separate',
              'Amber identifies capacitive contact. Red identifies a press or squeeze. A trigger touch is separate from trigger travel; a stick touch is separate from a click or direction. Available sensors depend on the controller profile. Contact binding and hardware detection are different checks.',
              ['Amber = touch; red = press', 'Touch does not click or squeeze', 'Sensor support depends on your controller'], selector='#tour-description'),
        scene('08-thumbrest', 'Give a thumb rest a useful job',
              'Thumb rests have no default action. A useful optional mapping is the left thumb rest as another way to hold open the wrist equipment picker. Add it as an alternative and choose While held. Lift your thumb to close the picker. Check accidental activation in your headset before adopting it.',
              ['Optional: wrist picker while touching L thumb rest', 'Retain left trigger as the first option', 'Lift your thumb to release'], selector='[data-row="1"][data-token="left_thumbrest"]'),
        scene('touch-0', 'Four face-button contacts',
              'A and B belong to the right controller; X and Y belong to the left. Each has a separate touch input. Light contact does not send its normal button press. These can be modifiers, but choose a combination you can release comfortably and intentionally.',
              ['Right: A touch, B touch', 'Left: X touch, Y touch', 'Contact remains separate from a press'], selector='#tour-description'),
        scene('touch-1', 'Stick tops are inputs too',
              'Each stick top has a touch input. Pressing inward is a stick click. Tilting is a direction or an axis. They are independently available in the editor. For repeated direction gestures, center the stick first. There are ten separate touch contacts in total.',
              ['Touch, click and direction are independent', 'Center before another direction gesture', 'Ten contact inputs are available'], selector='#tour-description'),
        scene('07-gesture', 'Choose what the gesture means',
              'While held stays active until you release. Press once runs at the start. On release runs when the input ends. Short tap finishes before its threshold. Long hold fires once after its threshold. A tap and hold sharing the same controls must use the same threshold.',
              ['While held: sustained input', 'Press / release: one edge', 'Tap / hold: shared timing threshold'], selector='#gesture-1'),
        scene('06-saved', 'Save, release, then verify',
              'Choose Save controls and tuning. The launcher validates the complete configuration and makes a backup. To apply a controls change live, release buttons, grips and triggers, center both sticks, and wait about two seconds. Also lift any touch contact used by the old or new layout. Runtime settings require a restart.',
              ['Save validates the complete layout', 'Release controls; center sticks for 2 seconds', 'Lift mapped touch contacts too'], selector='#status'),
        scene('11-conflict', 'Resolve a conflict before saving',
              'The validator rejects conflicting bindings and leaves the saved file intact. Read the named actions in the error, change the conflicting chord, or discard the draft. If another editor changed the file, discard and reload before trying again. Never assume a rejected draft is active in game.',
              ['Read the actions named in the error', 'Fix or discard the draft', 'Rejected saves leave the saved file intact'], selector='#status'),
        scene('09-sticks', 'Assign the sticks by context',
              'Stick assignments are separate from button actions. Left stick moves Snake without handheld focus and browses lists with focus. The focused right-stick left and right chords change tabs and consume map panning in those directions. Equipment and commands have their own assignments. Movement, turn, steering, pedals and native mode axes are listed here too.',
              ['Unfocused: L stick moves Snake', 'Focused: L stick browses; R stick changes tabs', 'Ten context-specific axis assignments'], selector='[data-axis="axes.menu"]'),
        scene('10-unbound', 'Optional does not mean missing',
              'An unbound action is deliberately inactive. Add an alternative to assign it, or Default to restore its default. Unbind action disables it. Fourteen optional shortcuts are off in this saved layout. Do not bind every optional native shortcut to the same buttons already used by ordinary VR controls.',
              ['Add an option to assign an unbound action', 'Default restores the shipped assignment', 'Keep Confirm and Back available'], selector='#add-alternative'),
        scene('tour-system-idroid', 'iDroid: open and stow',
              'In this layout, briefly tap the left Menu button to open or stow iDroid. Keep the tap shorter than five hundred and fifty milliseconds. A longer hold opens Pause, or native Help inside iDroid. The handheld device belongs in the left bionic hand; ordinary status stays on the left forearm.',
              ['Short L Menu tap: open / stow', 'Long L Menu hold: Pause / native Help', 'Handheld device: LEFT bionic hand'], selector='#tour-binding', action='system.idroid'),
        scene('tour-system-idroid_focus', 'iDroid: hold focus to operate',
              'With handheld iDroid open, hold left grip to focus its menu. Focus owns the sticks and face buttons. Release it to return to movement when the native state permits it. Focus must be a sustained While held binding. In spatial mode, the open native menu already owns menu controls.',
              ['Handheld: hold L grip to focus', 'Use While held for the focus binding', 'Release to restore permitted movement'], selector='#tour-binding', action='system.idroid_focus'),
        scene('tour-idroid-next_tab', 'iDroid: next page and navigation',
              'Hold left grip and flick the right stick right for the next tab. Center the stick before repeating. Left stick browses lists while focused. A confirms and B goes back. The existing right-grip page loop is still available: press once and release for each next page. Release left grip for left-stick movement.',
              ['Hold L grip + flick R stick right: Next tab', 'L stick browses; A confirms; B goes back', 'R grip: retained page loop'], selector='#tour-binding', action='idroid.next_tab'),
        scene('tour-idroid-previous_tab', 'iDroid: previous page is a chord',
              'Keep left grip held and flick the right stick left for the previous iDroid tab. Center before repeating. Left-stick click remains an alternative previous-tab chord. The ordinary Previous tab binding in other menus is a separate action. The actual-game segment later demonstrates the retained grip and stick-click alternatives.',
              ['Hold L grip + flick R stick left: Previous tab', 'Center the R stick before repeating', 'L stick click: retained alternative'], selector='#tour-binding', action='idroid.previous_tab'),
        scene('12-handheld-setting', 'Choose the iDroid presentation',
              'In VR settings, Handheld iDroid selects the physical left-hand presentation. With it off, iDroid uses the spatial menu panel. Pause remains spatial, and ordinary status remains on the left forearm. The fitting preview shows saved dimensions and offsets; verify device and screen motion together in your headset.',
              ['Handheld on: physical left-hand iDroid', 'Handheld off: spatial menu panel', 'Pause stays spatial; status stays left'], second='13-idroid-fit'),
    ]
    if editor_clip:
        scenes.insert(6, scene(None, 'Watch a complete remap',
                               'Here is the actual editor in motion. We keep right grip, add an alternative, choose Light touch, select right trigger touch, and remove the placeholder A. While held makes this a sustained action. Save, reload, and the control tour shows the saved touch binding.',
                               ['Original R grip stays available', 'New option: R trigger touch, While held', 'Save → reload → inspect the saved guide'], kind='launcher_video'))
    if clip:
        scenes.append(scene(None, 'iDroid in the actual game',
                            'This earlier left-eye simulator take demonstrates the retained alternatives. Left grip focuses the handheld. Right grip advances; left-stick click with focus goes back. Short Menu taps stow, reopen and stow. The right-stick tab chords shown in the guide are also available. Watch the hand and projection move together.',
                            ['Actual earlier take: retained grip / click alternatives', 'Move the hand and its projection together', 'Short Menu tap: stow → reopen → stow'], kind='gameplay'))
    scenes.extend([
        scene('mode-gameplay', 'On foot: move and fight',
              'On foot, the left stick moves and the right stick turns in this layout. A controls stance, B pickup or carry, and Y contextual interaction. Right grip readies the physical gun, right trigger fires or performs native close quarters actions, and left grip supports near the authored support socket. Reload and switch weapon use their chords.',
              ['Movement and turning have separate axes', 'Native actions depend on stance and equipment', 'Physical gun and support grip remain VR'], mode='gameplay'),
        scene('mode-binoculars', 'Optics: hold, aim, zoom, mark',
              'Equip physical binoculars with the saved left grip and Y hold. Release the chord, raise the ocular to your eye, and aim with your hand. Left stick click cycles lens zoom. Right trigger explicitly marks a visible target. A clears a mark, X requests native intel, and a fresh B press stows the binoculars.',
              ['Physical binoculars stay in your hand', 'Zoom changes the lens, not the world', 'Mark deliberately; B stows'], mode='binoculars'),
        scene('mode-equipment', 'Equipment lives on the left arm',
              'Hold left trigger to open the wrist picker. Choose a category, center the browsing stick, then choose an item card. Release to equip and close. Release without choosing to cancel. Use only appears where the native item permits it. Night vision is selected under Items; select None to turn it off.',
              ['Hold → category → center → card → release', 'Primary / Secondary / Support / Items', 'Night vision: Items; None turns it off'], mode='equipment'),
        scene('mode-commands', 'Commands: keep the menu held',
              'Hold X while choosing an available command. Browse with the commands stick and confirm with A. Release the hold to close. On horseback, use the separate mounted opening chord. An existing weapon or close quarters hold can continue, but opening Commands does not create a grab or make an unavailable command available.',
              ['Open and keep-open are separate actions', 'Hold while browsing and confirming', 'Native target and buddy availability still apply'], mode='commands'),
        scene('mode-horse', 'Horse controls have their own mode',
              'Horse mode uses its own movement and action mappings. X gallops here, so mounted Commands uses left trigger and X. Read the saved horse entries for stance, interaction, firing and equipment. Native availability still depends on the rider and weapon state. Stow a readied gun before dismounting.',
              ['Check the Horse context before remapping', 'X gallops; mounted Commands uses a chord', 'Stow the gun before dismounting'], mode='horse'),
        scene('mode-vehicle', 'Driving: wheel, pedals and weapons',
              'Grip with an empty hand near the wheel to steer physically. Both hands can join steering. With the gun stowed, triggers accelerate and brake. A readied personal gun owns its firing trigger, so the vehicle pedals stick supplies acceleration and braking. Native vehicle and seat restrictions still apply. The optional equipment-open shortcut is disabled by default.',
              ['Empty hand + wheel grip: physical steering', 'Gun ready: pedals axis owns driving', 'Check seat and weapon availability'], mode='vehicle'),
        scene('tour-system-pause', 'Pause and other native menus',
              'A long Menu hold opens Pause on a spatial stereo panel. Menu context includes confirm, back, page actions, triggers and clicks. These are the same physical VR inputs, interpreted by the active native screen. Release and center controls after a mode or focus change, then make a fresh input.',
              ['Pause uses the spatial stereo panel', 'Use native prompts in the Menu context', 'Release and center after ownership changes'], action='system.pause'),
        scene('mode-nativeButtons', 'Optional complete native mode',
              'Native controls is a separate optional input mode. Hold left Menu and right A for five hundred and fifty milliseconds to toggle it, then release. It provides the full native button set while ordinary VR action mappings are dormant. It changes input, not VR presentation. Toggle the same chord to return.',
              ['L Menu + A hold toggles native input', 'Release before using the new mode', 'Same chord returns to ordinary VR input'], mode='nativeButtons'),
        scene('02-reload', 'Export a guide for your own layout',
              'Save your edits, then choose a PNG control guide page and Export PNG. The four pages cover movement and fighting, iDroid and menus, optics and equipment, and driving and travel. The quick tour and guides resolve your current saved bindings. A remap changes the controller guide, not old recorded gameplay.',
              ['Save before exporting or touring', 'Four printable pages use saved bindings', 'Complete action index accompanies this video'], selector='#guide-page'),
        scene('mode-menus', 'Check the result before relying on it',
              f'The editor regression covers all {len(bindings["actions"])} actions, twenty nine inputs, ten touch contacts, ten stick assignments and all eight contexts. In game, verify the real action, release recovery and iDroid reopening. Full mission coverage, every weapon and vehicle, and physical touch sensor acceptance remain separate requirements. Consult the recorded regression results for the exact coverage.',
              ['All supported editor entries audited', 'Real game outcomes are recorded separately', 'Physical headset and whole-game checks remain open'], mode='menus'),
    ])
    actions = {a['name']: a for a in bindings['actions']}
    for row in scenes:
        if row.get('action'):
            row['mapping'] = ' OR '.join(label(b) for b in actions[row['action']]['bindings']) or 'Unbound'
        if row.get('mode'):
            row['mode_count'] = sum(row['mode'] in a['contexts'] for a in bindings['actions'])
    return scenes


def arrow(draw, start, end, color):
    draw.line([start, end], fill=color, width=5)
    angle = math.atan2(end[1] - start[1], end[0] - start[0])
    draw.polygon([end, (end[0]-19*math.cos(angle-.5), end[1]-19*math.sin(angle-.5)),
                  (end[0]-19*math.cos(angle+.5), end[1]-19*math.sin(angle+.5))], fill=color)


def crop_to_box(image, box, size):
    x, y, width, height = box
    image = image.crop((x, y, x+width, y+height))
    ratio = min(size[0]/image.width, size[1]/image.height)
    image = image.resize((round(image.width*ratio), round(image.height*ratio)), Image.Resampling.LANCZOS)
    tile = Image.new('RGB', size, PAPER)
    tile.paste(image, ((size[0]-image.width)//2, (size[1]-image.height)//2))
    return tile


def render_scene(row, number, total, base, captures, out):
    picture = Image.new('RGB', SIZE, INK)
    draw = ImageDraw.Draw(picture)
    draw.rectangle((48, 50, 55, 130), fill=RED)
    draw.text((82, 39), 'MGS5VR  /  CONTROLLER MAPPING', font=font(20, True), fill=MUTED)
    draw.text((82, 75), row['title'], font=font(42, True), fill=PAPER)
    draw.text((1730, 56), f'{number:02d} / {total:02d}', font=font(25, True), fill=AMBER)
    draw.line((48, 146, 1872, 146), fill='#536257', width=2)
    bounds=(48, 174, 1336, 1033)
    # The full UI remains visible. Enlargement is a separate callout inset.
    if row.get('shot'):
        source = Image.open(base / 'launcher' / (row['shot']+'.png')).convert('RGB')
        scaled = source.resize((1288, 859), Image.Resampling.LANCZOS)
        picture.paste(scaled, (48, 174))
        draw.rectangle(bounds, outline='#b6b09e', width=2)
    elif row['kind'] == 'gameplay':
        source = Image.new('RGB',(1440,960),INK)
        draw.rectangle(bounds, outline='#536257', width=2)
        guide = Image.open(base/'launcher/tour-system-idroid_focus.png').convert('RGB')
        tile = crop_to_box(guide,(862,303,548,573),(444,464))
        picture.paste(tile,(882,217)); draw.rectangle((881,216,1327,682),outline=AMBER,width=2)
        draw.text((882,180),'YOUR SAVED CONTROL / L GRIP',font=font(19,True),fill=AMBER)
        for y,text in [(718,'L grip: focus'),(757,'R grip: next'),(796,'L grip + L stick click: previous')]:
            words(draw,text,882,y,428,size=24,color=PAPER)
        draw.text((882,858),'ACTUAL LEFT EYE',font=font(20,True),fill=AMBER)
        words(draw,'Meta XR Simulator capture: about 3.6 frames per second. This measures capture sampling, not headset frame rate.',882,896,428,size=20,color=MUTED)
    else:
        source = Image.new('RGB',(1440,960),PAPER)
        draw.rectangle(bounds, outline='#b6b09e', width=2)
    sidebar_x=1374
    draw.text((sidebar_x, 181), 'FOLLOW THESE STEPS', font=font(19, True), fill=AMBER)
    y=230
    for i, text in enumerate(row['bullets']):
        draw.rounded_rectangle((sidebar_x,y+5,sidebar_x+30,y+35),radius=7,fill=RED)
        draw.text((sidebar_x+9,y+4),str(i+1),font=font(19,True),fill=PAPER)
        y=words(draw,text,sidebar_x+46,y,430,size=26,color=PAPER)+22
    if row.get('mapping'):
        y+=10
        draw.text((sidebar_x,y),'SAVED BINDING',font=font(17,True),fill=MUTED)
        y=words(draw,row['mapping'],sidebar_x,y+30,480,size=25,color=AMBER,bold=True)
    if row.get('mode_count'):
        y+=15
        words(draw,f"{row['mode_count']} actions in this context. Full catalog in the companion index.",sidebar_x,y,480,size=22,color=MUTED)
    highlight=None
    if row['kind'] == 'launcher_video':
        draw.text((sidebar_x,713),'LIVE ZOOM / ACTUAL EDITOR',font=font(18,True),fill=AMBER)
        draw.rectangle((sidebar_x-2,748,1856,942),outline=AMBER,width=4)
    if row.get('selector') and row.get('shot'):
        capture=captures[row['shot']]
        boxes=[b for b in capture['boxes'] if b['selector']==row['selector'] and b['y']<960 and b['y']+b['height']>0]
        if boxes:
            b=boxes[0]
            # Cap the inset height, keeping actual control text at its center.
            w=min(1440,max(220,b['width']+34));h=min(300,max(110,b['height']+45))
            x=max(0,min(1440-w,b['x']+b['width']/2-w/2));yy=max(0,min(960-h,b['y']+b['height']/2-h/2))
            inset=crop_to_box(source,(x,yy,w,h),(480,190))
            picture.paste(inset,(sidebar_x,750));draw.rounded_rectangle((sidebar_x-2,748,1856,942),radius=2,outline=AMBER,width=4)
            draw.text((sidebar_x,713),'ZOOM / ACTUAL EDITOR',font=font(18,True),fill=AMBER)
            scale=1288/1440
            highlight=(48+b['x']*scale,174+b['y']*scale,48+(b['x']+b['width'])*scale,174+(b['y']+b['height'])*scale)
            draw.rectangle(highlight,outline=RED,width=4)
            point=((highlight[0]+highlight[2])/2,(highlight[1]+highlight[3])/2)
            arrow(draw,(sidebar_x-14,810),(min(1325,point[0]),max(185,min(1000,point[1]))),AMBER)
    if row.get('second'):
        other=Image.open(base/'launcher'/(row['second']+'.png')).convert('RGB')
        # Split-screen comparison, each panel retains its actual captured UI.
        titles=('VR SETTINGS / HANDHELD','FITTING PREVIEW') if row['shot']=='12-handheld-setting' else ('ORIGINAL / FIRST OPTION','ADDED ALTERNATIVE')
        for image, x, title in [(source,48,titles[0]),(other,714,titles[1])]:
            tile=crop_to_box(image,(185,340,1255,620),(622,658))
            picture.paste(tile,(x,350));draw.rectangle((x,350,x+622,1008),outline=AMBER,width=2)
            draw.text((x+15,310),title,font=font(19,True),fill=AMBER)
        draw.line((691,350,691,1008),fill=RED,width=6)
    source_label='LAUNCHER UI / DEMONSTRATION COPY OF SAVED CONTROLS' if row['kind'].startswith('launcher') else 'SIMULATOR FINAL LEFT EYE / PHYSICAL HEADSET ACCEPTANCE SEPARATE'
    draw.text((65,151),source_label,font=font(13,True),fill=MUTED)
    picture.save(out)
    row['highlight']=highlight


def subtitles(text, duration):
    tokens=text.split()
    groups=[' '.join(tokens[i:i+12]) for i in range(0,len(tokens),12)]
    total=sum(len(g.split()) for g in groups)
    pos=0
    rows=[]
    for group in groups:
        end=pos+len(group.split())/total*duration
        rows.append({'start':pos+.65,'end':end+.65,'text':group})
        pos=end
    return rows


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=ROOT/'artifacts/dev/controller-mapping')
    parser.add_argument('--clip',type=Path,help='Separately reviewed final-compositor-eye MP4')
    parser.add_argument('--editor-clip',type=Path,help='Actual recorded browser remap, using a disposable settings copy')
    parser.add_argument('--prepare',action='store_true')
    parser.add_argument('--render',action='store_true')
    args=parser.parse_args()
    base=args.output.resolve();work=base/'film';work.mkdir(exist_ok=True,parents=True)
    bindings=json.loads((base/'effective-bindings.json').read_text())
    regression=json.loads((base/'launcher-regression.json').read_text())
    script_path=work/'script.json'
    if args.prepare:
        scenes=make_script(bindings,args.clip,args.editor_clip)
        for i,row in enumerate(scenes):
            row['audio']=str(work/f'{i+1:02d}.wav')
        script={'schema':1,'source':'actual launcher screenshots and recorded browser interactions with authored explanations','synthetic_narration':True,'scenes':scenes,'clip':str(args.clip.resolve()) if args.clip else None,'editor_clip':str(args.editor_clip.resolve()) if args.editor_clip else None}
        script_path.write_text(json.dumps(script,indent=2),encoding='utf-8')
        subprocess.run(['powershell.exe','-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',str(ROOT/'tools/narrate-controller-mapping.ps1'),'-ScriptPath',str(script_path)],check=True)
        captures={c['name']:c for c in regression['captures']}
        start=0
        for i,row in enumerate(scenes):
            with wave.open(row['audio'],'rb') as stream:
                audio=stream.getnframes()/stream.getframerate()
            row.update(audio_seconds=audio,duration=max(8,math.ceil((audio+2.25)*30)/30),start=start)
            if row['kind'] in ('gameplay','launcher_video'):
                media=script['clip'] if row['kind']=='gameplay' else script['editor_clip']
                length=float(subprocess.check_output(['ffprobe','-v','error','-show_entries','format=duration','-of','default=noprint_wrappers=1:nokey=1',media],text=True))
                row['media']=media
                row['media_start']=8.4 if row['kind']=='gameplay' else 0
                row['media_end']=min(length,31.5) if row['kind']=='gameplay' else length
                row['duration']=math.floor((row['media_end']-row['media_start'])*30)/30
                if audio+.65>row['duration']:
                    raise ValueError('Narration must finish within the actual moving recording: '+row['title'])
            row['subtitles']=subtitles(row['narration'],audio)
            row['frame']=str(work/f'{i+1:02d}.png')
            render_scene(row,i+1,len(scenes),base,captures,Path(row['frame']))
            start+=row['duration']
        script['duration_seconds']=start
        script_path.write_text(json.dumps(script,indent=2),encoding='utf-8')
        export_index(bindings,base,scenes)
        print(json.dumps({'scenes':len(scenes),'duration_seconds':start,'script':str(script_path)}),flush=True)
    if args.render:
        script=json.loads(script_path.read_text());scenes=script['scenes']
        final=base/'controller-mapping-showcase.mp4'
        encoder=subprocess.Popen(['ffmpeg','-hide_banner','-loglevel','error','-y','-f','rawvideo','-pixel_format','rgb24','-video_size','1920x1080','-framerate','30','-i','pipe:0','-an','-c:v','libx264','-threads','4','-preset','fast','-crf','19','-pix_fmt','yuv420p',str(work/'picture.mp4')],stdin=subprocess.PIPE)
        previous=None;clip_reader=None
        try:
            for i,row in enumerate(scenes):
                still=Image.open(row['frame']).convert('RGB');total=round(row['duration']*30)
                if row.get('media'):
                    live_size=(816,854) if row['kind']=='gameplay' else (1288,859)
                    live_position=(48,174)
                    # Normalize the original timestamp grid before trimming.
                    # Seeking a sparse VFR input first can shift the new origin
                    # to the next source sample and shorten its chapter.
                    first=round(row['media_start']*30)
                    vf=(f'scale={live_size[0]}:{live_size[1]}:force_original_aspect_ratio=decrease,'
                        f'pad={live_size[0]}:{live_size[1]}:(ow-iw)/2:(oh-ih)/2,fps=30,'
                        f'trim=start_frame={first}:end_frame={first+total},setpts=PTS-STARTPTS')
                    clip_reader=subprocess.Popen(['ffmpeg','-hide_banner','-loglevel','error','-i',row['media'],'-vf',vf,'-f','rawvideo','-pix_fmt','rgb24','pipe:1'],stdout=subprocess.PIPE)
                live_frame=None
                for frame in range(total):
                    t=frame/30;picture=still.copy()
                    if clip_reader is not None:
                        count=live_size[0]*live_size[1]*3
                        data=clip_reader.stdout.read(count)
                        if len(data)!=count:
                            raise RuntimeError('Recorded media ended before its declared chapter; refusing to freeze it: '+row['title'])
                        live_frame=Image.frombytes('RGB',live_size,data)
                        picture.paste(live_frame,live_position)
                        if row['kind']=='launcher_video':
                            # The inset enlarges the same moving editor pixels.
                            original=live_frame.resize((1440,960),Image.Resampling.LANCZOS)
                            inset=crop_to_box(original,(277,425,567,230),(480,190))
                            picture.paste(inset,(1374,750))
                            ImageDraw.Draw(picture).rectangle((1372,748,1856,942),outline=AMBER,width=4)
                    draw=ImageDraw.Draw(picture)
                    # A short directional wipe makes chapter changes legible.
                    if previous is not None and not row.get('media') and t<.6:
                        reveal=int(1920*(t/.6)**.65)
                        picture.paste(previous.crop((reveal,0,1920,1080)),(reveal,0))
                        draw.line((reveal,0,reveal,1080),fill=AMBER,width=5)
                    # Always-readable captions and chapter progress.
                    cue=next((s for s in row['subtitles'] if s['start']<=t<s['end']),None)
                    if cue:
                        # Captions stay below the full UI; never cover Save or
                        # the bottom native page prompts being demonstrated.
                        draw.rounded_rectangle((48,1035,1872,1065),radius=7,fill=INK)
                        face=font(23,True);width=draw.textlength(cue['text'],font=face)
                        if width>1770:face=font(21,True);width=draw.textlength(cue['text'],font=face)
                        draw.text((960-width/2,1035),cue['text'],font=face,fill=PAPER)
                    progress=(row['start']+t)/script['duration_seconds']
                    draw.rectangle((0,1067,int(1920*progress),1079),fill=RED)
                    encoder.stdin.write(picture.tobytes())
                previous=picture
                if clip_reader is not None:
                    clip_reader.stdout.close()
                    if clip_reader.poll() is None:clip_reader.terminate()
                    clip_reader.wait();clip_reader=None
                print(f'Encoded chapter {i+1}/{len(scenes)}: {row["title"]}',flush=True)
        finally:
            if clip_reader is not None:clip_reader.terminate();clip_reader.wait()
            encoder.stdin.close();code=encoder.wait()
        if code:raise RuntimeError('Picture encoding failed')
        # Build narration with deliberate short silence at chapter boundaries.
        audio_rows=[]
        for i,row in enumerate(scenes):
            target=work/f'{i+1:02d}-padded.wav'
            subprocess.run(['ffmpeg','-hide_banner','-loglevel','error','-y','-i',row['audio'],'-af','adelay=650,apad','-t',str(row['duration']),'-ar','48000','-ac','2',str(target)],check=True)
            audio_rows.append("file '"+target.as_posix().replace("'","'\\''")+"'")
        (work/'audio.txt').write_text('\n'.join(audio_rows)+'\n')
        metadata=[';FFMETADATA1','title=MGS5VR — Controller mapping walkthrough','comment=Actual launcher UI; demonstrations use a disposable settings copy. Synthetic narration. Simulator footage is separately labeled.']
        for row in scenes:
            metadata.extend(['[CHAPTER]','TIMEBASE=1/1000',f'START={round(row["start"]*1000)}',f'END={round((row["start"]+row["duration"])*1000)}','title='+row['title']])
        (work/'chapters.txt').write_text('\n'.join(metadata),encoding='utf-8')
        subprocess.run(['ffmpeg','-hide_banner','-loglevel','error','-y','-i',str(work/'picture.mp4'),'-f','concat','-safe','0','-i',str(work/'audio.txt'),'-i',str(work/'chapters.txt'),'-map','0:v','-map','1:a','-map_metadata','2','-map_chapters','2','-c:v','copy','-c:a','aac','-b:a','160k','-movflags','+faststart',str(final)],check=True)
        manifest={'schema':1,'video':str(final),'video_sha256':digest(final),'duration_seconds':script['duration_seconds'],'width':1920,'height':1080,'fps':30,'narration':'Microsoft Zira Desktop synthetic speech','source':'actual launcher UI fixture captures and recorded browser remap; authored arrows, zoom boxes, side-by-side comparisons and directional chapter wipes','editor_clip':script.get('editor_clip'),'editor_clip_sha256':digest(script['editor_clip']) if script.get('editor_clip') else None,'gameplay_clip':script.get('clip'),'gameplay_clip_sha256':digest(script['clip']) if script.get('clip') else None,'gameplay_capture_fps_is_not_video_export_fps':True,'controls_sha256':regression['personal_controls_sha256'],'launcher_regression_sha256':digest(base/'launcher-regression.json'),'scenes':[{k:v for k,v in row.items() if k not in ('subtitles','highlight')} for row in scenes]}
        (base/'video-manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
        print(json.dumps({'video':str(final),'seconds':script['duration_seconds']}),flush=True)


def export_index(bindings,base,scenes):
    with (base/'complete-control-index.csv').open('w',newline='',encoding='utf-8-sig') as stream:
        writer=csv.writer(stream);writer.writerow(['Action','Saved binding','Contexts','Editor audit','Real game acceptance'])
        for action in bindings['actions']:
            writer.writerow([action['name'],' OR '.join(label(b) for b in action['bindings']) or 'Unbound',' / '.join(action['contexts']),'Listed and physical location checked','See regression report; not implied by editor audit'])
        for axis in bindings['axes']:
            writer.writerow([axis['name'],axis['source'],'Axis assignment','Save/read checked','See regression report'])
    def clock(seconds):return f'{int(seconds)//60:02d}:{int(seconds)%60:02d}'
    chapters='\n'.join(f"{clock(row['start'])}  {row['title']}" for row in scenes)
    (base/'video-chapters.txt').write_text(chapters+'\n',encoding='utf-8')
    (base/'narration.txt').write_text('\n\n'.join(clock(row['start'])+' '+row['title']+'\n'+row['narration'] for row in scenes),encoding='utf-8')


if __name__=='__main__':main()
