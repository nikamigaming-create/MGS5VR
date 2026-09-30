"""Build the review edition of the field kit from evidence and effective controls."""
import argparse
from collections import defaultdict
import html
import json
import math
from pathlib import Path

from reportlab.lib import colors
from reportlab.lib.enums import TA_LEFT
from reportlab.lib.pagesizes import landscape, A4
from reportlab.lib.styles import ParagraphStyle
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, PageBreak, Table, TableStyle, Image, Flowable, KeepTogether

ROOT = Path(__file__).resolve().parents[2]
PAPER, INK, RED = map(colors.HexColor, ('#EEE9DF', '#292B29', '#AE382E'))
WIDTH, HEIGHT = landscape(A4)
CONTENT = WIDTH - 84


def read(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def clean(value):
    return html.escape(str(value).replace('\u2011','-').replace('\u2013','-').replace('\u2014',' - '))


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--coverage',type=Path,required=True)
    parser.add_argument('--atlas',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--run',type=Path,required=True)
    parser.add_argument('--supervised-run',type=Path)
    parser.add_argument('--idroid-run',type=Path)
    args=parser.parse_args()
    for name,file in [('Body','arial.ttf'),('Bold','arialbd.ttf'),('Condensed','arialnb.ttf')]:
        pdfmetrics.registerFont(TTFont(name, str(Path('C:/Windows/Fonts')/file)))
    styles={
        'title':ParagraphStyle('title',fontName='Condensed',fontSize=42,leading=42,textColor=INK,spaceAfter=18),
        'h2':ParagraphStyle('h2',fontName='Condensed',fontSize=23,leading=26,textColor=INK,spaceAfter=13),
        'body':ParagraphStyle('body',fontName='Body',fontSize=10,leading=14,textColor=INK,spaceAfter=9),
        'small':ParagraphStyle('small',fontName='Body',fontSize=8,leading=11,textColor=INK,spaceAfter=6),
        'label':ParagraphStyle('label',fontName='Bold',fontSize=8,leading=11,textColor=RED,spaceAfter=9),
    }
    def para(value,style='body'):
        return Paragraph(clean(value),styles[style])
    story=[]
    def heading(number,title,sub=None):
        story.extend([para('FIELD KIT / '+number,'label'),para(title,'title')])
        if sub:story.append(para(sub))
    def page():story.append(PageBreak())
    def table(rows,widths):
        values=[[para(cell,'small') for cell in row] for row in rows]
        t=Table(values,colWidths=widths,repeatRows=1,hAlign='LEFT')
        t.setStyle(TableStyle([('VALIGN',(0,0),(-1,-1),'TOP'),('BACKGROUND',(0,0),(-1,0),colors.HexColor('#DED7CA')),
            ('LINEBELOW',(0,0),(-1,0),1,RED),('LINEBELOW',(0,1),(-1,-1),.35,colors.HexColor('#BDB5A8')),
            ('LEFTPADDING',(0,0),(-1,-1),8),('RIGHTPADDING',(0,0),(-1,-1),8),
            ('TOPPADDING',(0,0),(-1,-1),6),('BOTTOMPADDING',(0,0),(-1,-1),6)]))
        return t
    report=read(args.coverage); bindings=read(args.coverage.with_name('effective-bindings.json'))
    run=read(args.run/'result.json'); atlas=read(args.atlas/'atlas.json')
    callouts=read(Path(__file__).parent/'assets/controller-callouts.json')
    for plate in atlas['plates']:
        extra=callouts.get(plate['image'])
        if extra:
            if extra['image_sha256']!=plate['image_sha256']:
                raise ValueError('Re-review rendered Menu-button location before using the callout on a changed image')
            plate['anchors'].update(extra['anchors'])
    supervised=read(args.supervised_run/'result.json') if args.supervised_run else {}
    idroid=read(args.idroid_run/'result.json') if args.idroid_run else {}
    idroid_case=next(iter(idroid.get('cases',[])),{})
    idroid_pass=idroid_case.get('status')=='observed_pass'
    cases=[case for suite in run.get('suites',[]) for case in suite.get('cases',[])]
    native_pass=sum(case['status']=='observed_pass' for case in cases)
    heading('27 SEP 2026 / VERIFICATION EDITION','MGS5VR\nFIELD KIT'.replace('\n',' / '))
    story.append(para('Candidate evidence, controller reference and the complete current issue register.','h2'))
    arrival=run.get('arrival',{}).get('captures',[])
    hero=Image(arrival[0],width=245,height=257) if arrival else para('Fresh gameplay arrival image unavailable.')
    cover=[para('WORK IN PROGRESS','label'),para(f'{native_pass} native action checks passed in the latest campaign. Visual review and headset acceptance remain separate.'),
           para('55 community reports / 35 situations / 26 recovered feature families / 96 configured actions.'),
           para('The 899 equipment records are a recovered definition inventory. Access and behavior are not established for every record.'),
           para('Public release: experimental-2026-09-24. The local candidate is not a published patch.'),
           para('This edition records the work honestly. It is not the finished all-feature instructional film.','label')]
    story.append(Table([[hero,cover]],colWidths=[270,CONTENT-270],style=[('VALIGN',(0,0),(-1,-1),'TOP')]))
    page();heading('01 / EVIDENCE','Where the patch stands')
    story.append(para('Coco\'s files were reviewed as contributions against the current checkout. Wholesale replacement would remove newer work. The current-shot camera intent was adapted; his exact two-second stale-demo recovery is not in this candidate. Mission 6 is still open.'))
    story.append(table([['Layer','What this edition establishes','Remaining gate'],
        ['Source and contracts','Palm display and hologram share one anatomical mount; bot records build/configuration identity and current skin publication.','Native geometry and final-eye motion must agree.'],
        ['Fresh campaign',f'{native_pass} individual native actions passed; every failed and unrun case is retained.','Complete each report\'s acceptance, then review both eyes.'],
        ['iDroid',('Normal map: six palm poses, both sticks, 0.000 m measured drift and Back exit passed.' if idroid_pass else 'Fresh camera diagnostics expose frozen native palms during tutorial/pause.'),'Paused tutorial hands, panel clipping, motion readability and headset fit.'],
        ['Showcase','A bounded binocular equip lesson uses the official left controller model and audited input timings.','All other lessons, matching action feedback, full walkthrough and cinematic assembly.']],[105,325,CONTENT-430]))
    story.append(Spacer(1,16));story.append(para('Candidate DLL SHA-256','label'));story.append(para(report['target_identity']['dll_sha256'],'small'))
    story.append(para('Run: '+args.run.name+' / '+run['status']+'. Controller settings are the installed personal configuration exported by the compiled resolver.','small'))
    page()

    if supervised:
        heading('01 / LIVE SUPERVISION','Routine tests keep moving')
        story.append(para('The old worker reported a waiting state but could not choose the next input. A reviewed plan now runs through its known steps on one input connection. Unexpected transitions stop dependent steps and produce paired eye images after a two-second observation window.'))
        rows=[c for c in supervised.get('cases',[]) if c.get('decision_id')=='field-posture-continuous-01']
        story.append(para(f'{sum(c["status"]=="observed_pass" for c in rows)} / {len(rows)} native checks in 28.109 seconds','h2'))
        story.append(table([['Test','Native outcome']]+[[c['id'],c['status'].replace('_',' ')] for c in rows],[CONTENT*.72,CONTENT*.28]))
        story.append(para('No model waits occurred between these ten cases. The model reviews new blockers; it is not a one-second real-time controller. A waiting worker, completed queue, recorder finalization and a failed game transition are different states.','small'))
        page()

    class Plate(Flowable):
        def __init__(self,row,size,marks):super().__init__();self.row=row;self.width=self.height=size;self.marks=marks
        def draw(self):
            c=self.canv;c.drawImage(str(args.atlas/self.row['image']),0,0,self.width,self.height,mask='auto')
            for number in self.marks:
                point=self.row['anchors'][number];x=point['x']*self.width;y=(1-point['y'])*self.height
                c.setFillColor(PAPER);c.setStrokeColor(RED);c.setLineWidth(1.4);c.circle(x,y,9,fill=1,stroke=1)
                c.setFont('Bold',9);c.setFillColor(RED);c.drawCentredString(x,y-3,number)
    for hand in ('left','right'):
        heading('02 / CONTROLLER ATLAS',hand.title()+' Touch Plus','Enlarged views of the pinned WebXR model. Numbered spots map directly to the controls in the following register. These are illustrative model views.')
        face=next(x for x in atlas['plates'] if x['hand']==hand and x['view']=='face')
        side=next(x for x in atlas['plates'] if x['hand']==hand and x['view']=='side')
        legend=[para('CONTROL LOCATIONS','label'),para('1  '+('X' if hand=='left' else 'A')+' button'),
            para('2  '+('Y' if hand=='left' else 'B')+' button'),para('3  Thumbstick: direction + click'),
            para('4  Index trigger'),para('5  Middle-finger grip / squeeze'),para('6  Thumb-rest touch surface'),
            para('7  Left Menu button' if hand=='left' else 'The right system button is reserved by the runtime. Use the left Menu input for the bindings shown here.','small')]
        story.append(Table([[Plate(face,265,['1','2','3','6']+(['7'] if hand=='left' else [])),Plate(side,265,['4','5']),legend]],
                           colWidths=[265,265,CONTENT-530],style=[('VALIGN',(0,0),(-1,-1),'TOP')]))
        story.append(para('Source: immersive-web/webxr-input-profiles, revision '+atlas['source_commit']+'. MIT license retained with the models.','small'));page()

    heading('03 / IDROID','Raise the palm. Keep Snake still.')
    story.append(para('Target behavior: the native device sits in the raised right palm. Its upright hologram stays at a fixed physical offset from the device as the hand moves and rotates. Head and hands remain tracked; the sticks operate the iDroid while Snake stays stationary.'))
    story.append(table([['Check','Current evidence','Acceptance needed'],
        ['Display axes','Projection now uses the same anatomical basis as the palm-mounted device.','Correct native housing, normal, up axis and readable distance in both eyes.'],
        ['Hand movement','Normal map: native palms translated and rotated across six poses. Both-eye stills show matching panel movement. Tutorial/pause still freezes the skin.','Continuous motion, stable attachment and paused-tutorial behavior.'],
        ['Movement policy',('Latest normal-map probe passed both sticks, neutral settling and 0.000 m measured displacement.' if idroid_pass else 'Distinct native samples and neutral settling required for both sticks.'),'Readable menu response, physical headset comfort and tracking.'],
        ['Exit','Normal map opens and closes with Back. Held-Back tutorial suspension can leave a native handset out after the UI closes.','Finish tutorials normally; distinguish modal, terminal UI and native handset states.']],[105,325,CONTENT-430]))
    if args.idroid_run:story.append(para('Latest dedicated run: '+args.idroid_run.name+' / '+idroid.get('status','unavailable')+'.','small'))
    story.append(para('Do not call the iDroid fixed from the axis change or a stationary screenshot. The frozen-hand failure is an explicit release blocker.','label'));page()

    history=[r for r in report['rows'] if r['kind']=='historical_feature']
    for start in range(0,len(history),7):
        heading('04 / FEATURE REGISTER','Every recovered VR feature',f'Families {start+1}-{min(start+7,len(history))} of {len(history)}. Historical support is not automatically a current-candidate pass.')
        rows=[['Feature','Evidence state','Source / proof still required']]
        for row in history[start:start+7]:
            rows.append([row['title'],row['status'].replace('_',' '),row['source']+'; current native outcome, both-eye motion review and a matching teaching clip.'])
        story.append(table(rows,[290,135,CONTENT-425]));page()

    groups=defaultdict(list)
    for action in bindings['actions']:groups[action['name'].split('.')[0]].append(action)
    for group,actions in groups.items():
        size=math.ceil(len(actions)/math.ceil(len(actions)/14))
        for start in range(0,len(actions),size):
            title=group.replace('_',' ').title()+(' / continued' if start else '')
            heading('05 / EFFECTIVE CONTROLS',title, 'Installed bindings. LEVEL follows the button while pressed; PRESS fires on its down edge. TAP and HOLD split at the stated boundary.')
            rows=[['Action','Controller input','Eligible contexts']]
            for action in actions[start:start+size]:
                values=[]
                for binding in action['bindings']:
                    values.append(binding['gesture'].upper()+' '+' + '.join(binding['inputs']).replace('_',' ')+
                                  (f" ({binding['milliseconds']} ms boundary)" if binding['gesture'] in ('tap','hold') else ''))
                rows.append([action['name'],' OR '.join(values) or 'DISABLED',', '.join(action['contexts'])])
            story.append(table(rows,[240,270,CONTENT-510]));page()
    heading('05 / EFFECTIVE AXES','Stick ownership')
    story.append(table([['Axis','Configured source']]+[[x['name'],x['source']] for x in bindings['axes']],[CONTENT*.55,CONTENT*.45]))
    story.append(para('Tap and hold are distinct actions. Release all chord buttons, triggers and grips, and center the sticks before applying a changed binding. Disabled tutorial D-pad routes remain visible in the menu controls table.','small'));page()

    issues=[r for r in report['rows'] if r['kind']=='report']
    for start in range(0,len(issues),8):
        heading('06 / COMMUNITY REGISTER','Issues stay on the checklist',f'Reports {start+1}-{min(start+8,len(issues))} of {len(issues)}. No report is closed by this document.')
        rows=[['ID / priority','Reported behavior','Current evidence']]
        for row in issues[start:start+8]:rows.append([row['id']+' / '+row['priority'],row['title'],row['status'].replace('_',' ')])
        story.append(table(rows,[80,CONTENT-240,160]));page()
    heading('07 / PROOF RULES','What earns a check mark')
    for text in [
        '1. Record the exact executable, DLL and both configuration hashes. Validate the native scene before dispatching an input.',
        '2. Use the effective binding and observe a new native outcome. Retain distinct samples and release all inputs, including on failure.',
        '3. Join each image and clip to that case and build. Keep final compositor evidence distinct from the recorded native source eye.',
        '4. Review both eyes during motion, readability, contact and transitions. A compositor image existing is not a visual pass.',
        '5. Finish the required scenario, neutral exit and reopening checks. Physical-headset acceptance is recorded separately.',
        '6. Produce the teaching clip with the correct controller spots and timed cues. Only then close the complete report after review.'
    ]:story.append(para(text))
    story.append(para('Evidence files','h2'))
    story.append(para('Searchable ledger: artifacts/field-guide-20260927/verification/index.html. Case records and media: artifacts/bot-20260927/. Bounded binocular lesson: artifacts/field-guide-20260927/lessons/binoculars-equip-v3/lesson.mp4.','small'))
    story.append(para('Controller source: https://github.com/immersive-web/webxr-input-profiles/tree/'+atlas['source_commit']+'/packages/assets/profiles/meta-quest-touch-plus','small'))
    def chrome(canvas,doc):
        canvas.setFillColor(PAPER);canvas.rect(0,0,WIDTH,HEIGHT,fill=1,stroke=0)
        canvas.setStrokeColor(RED);canvas.setLineWidth(2);canvas.line(42,HEIGHT-30,WIDTH-42,HEIGHT-30)
        canvas.setFillColor(INK);canvas.setFont('Bold',8);canvas.drawString(42,HEIGHT-22,'MGS5VR   /   FIELD KIT')
        canvas.setFont('Body',7);canvas.drawRightString(WIDTH-42,HEIGHT-22,'VERIFICATION EDITION   -   27 SEPTEMBER 2026')
        canvas.setFont('Body',7);canvas.drawString(42,20,'CANDIDATE WORK IN PROGRESS  /  PUBLIC RELEASE REMAINS 24 SEPTEMBER')
        canvas.drawRightString(WIDTH-42,20,f'{doc.page:02d}')
    args.output.parent.mkdir(parents=True,exist_ok=True)
    doc=SimpleDocTemplate(str(args.output),pagesize=(WIDTH,HEIGHT),leftMargin=42,rightMargin=42,topMargin=51,bottomMargin=38,
                          title='MGS5VR Field Kit - Verification Edition',author='MGS5VR')
    doc.build(story,onFirstPage=chrome,onLaterPages=chrome)
    print(args.output.resolve())


if __name__=='__main__':main()
