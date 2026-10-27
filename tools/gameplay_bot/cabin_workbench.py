"""One bounded actual-game cassette, camera, note and board interaction loop."""
import math
import time
from .core import BotFault, atomic_json
from .live import rotate
from .combat import inverse, mul, pointing
from .motion import _blend
from .session import preserved_tracking_poses, _controller_pose
from .startup import advance_startup, wait_for_continue_rack


def compose(parent, child):
    offset=rotate(parent['orientation'],child['position'])
    return {'position':[a+b for a,b in zip(parent['position'],offset)],
            'orientation':mul(parent['orientation'],child['orientation'])}


def shifted(pose, offset):
    return compose(pose,{'position':offset,'orientation':[0.,0.,0.,1.]})


def inverse_pose(pose):
    q=inverse(pose['orientation'])
    return {'position':rotate(q,[-p for p in pose['position']]),'orientation':q}


def workbench(live, timeout=90., film=False, on_ready=None):
    state=wait_for_continue_rack(live,advance_startup(live,timeout=timeout))
    ready_until=time.monotonic()+30.
    while not (state.get('opening_focus',{}).get('ready')
            and state.get('opening_bench',{}).get('layout',{}).get('verified')):
        if time.monotonic()>ready_until or state.get('scene')!='title':
            raise BotFault('The physical native cabin did not finish becoming ready')
        time.sleep(.1);state=live.observe()
    if state.get('opening_native_menu'):
        live.execute({'op':'action','name':'system.idroid'})
    until=time.monotonic()+5.
    while state.get('opening_native_menu') or not state.get('rendered',{}).get('studio',{}).get('available'):
        if time.monotonic()>until:raise BotFault('Optional prop mode did not open')
        time.sleep(.05);state=live.observe()
    if state.get('scene')!='title' or 3 in state.get('opening_focus',{}).get('actions',[]):
        raise BotFault('Workbench uses an unavailable GZ import tape; this title does not admit that safe case')
    # A requested source-eye/audio recording starts only after the native
    # workbench is ready and before the first scripted interaction. The
    # recorder independently verifies its first frame and fails closed.
    if on_ready:on_ready()
    captures=[];outcomes=[];started=time.monotonic()
    movie=None
    if film:
        from .cabin_film import CabinFilm
        movie=CabinFilm(live)
    aim_from_grip={}
    for hand in ('left','right'):
        grip=_controller_pose(live,hand,'grip','local')
        aim=_controller_pose(live,hand,'aim','local')
        aim_from_grip[hand]=compose(inverse_pose(grip),aim)
    def observe():
        s=live.observe()
        until=time.monotonic()+2.
        while s.get('scene')=='title' and not s.get('opening_focus',{}).get('ready'):
            # The extra native camera render can briefly outlive the title
            # publication's freshness window. Wait released of new actions;
            # do not replay the shutter or accept a different scene owner.
            if time.monotonic()>until:break
            time.sleep(.03);s=live.observe()
        if s.get('scene')!='title' or not s.get('opening_focus',{}).get('ready'):
            live.events.emit('workbench_owner_changed',state=s)
            raise BotFault('Native title ownership changed during the reversible workbench loop')
        return s
    def studio():
        value=(observe().get('rendered') or {}).get('studio') or {}
        if not value.get('available') or not value.get('cabin'):raise BotFault('Source-published studio is unavailable')
        return value
    def wait(predicate,label,seconds=5.):
        end=time.monotonic()+seconds
        while time.monotonic()<end:
            s=observe()
            if predicate(s):
                outcomes.append({'phase':label,'state':s})
                atomic_json(live.events.output/'workbench-progress.json',{'outcomes':outcomes,'captures':captures})
                return s
            time.sleep(.04)
        raise BotFault('Workbench did not observe '+label)
    def move(hand, target, seconds=.40):
        if movie:seconds=max(seconds,.70)
        before=_controller_pose(live,hand,'grip','local');began=time.monotonic()
        for n in range(1,7):
            pose=_blend(before,target,n/6)
            for kind,value in (('grip',pose),('aim',compose(pose,aim_from_grip[hand]))):
                live.call('set_controller_pose',{'hand':hand,'pose_type':kind,'base_space':'local',**value})
            time.sleep(max(0.,began+seconds*n/6-time.monotonic()))
    def aim_at(hand,target,seconds=.40):
        move(hand,compose(target,inverse_pose(aim_from_grip[hand])),seconds)
    def approach(target,offset=(0.,.65,.32)):
        # Move the observer into ordinary arm reach before touching native
        # furniture. This is tracked room motion, never a two-meter arm.
        before=live.text_result(live.call('get_head_pose',{'base_space':'local'}))['pose']
        position=[a+b for a,b in zip(target['position'],offset)]
        end={'position':position,'orientation':pointing([a-b for a,b in zip(target['position'],position)])}
        delta=[a-b for a,b in zip(position,before['position'])]
        hands={hand:_controller_pose(live,hand,'grip','local') for hand in ('left','right')}
        for n in range(1,9):
            live.call('set_head_pose',{'base_space':'local',**_blend(before,end,n/8)})
            for hand,original in hands.items():
                pose={**original,'position':[a+b*n/8 for a,b in zip(original['position'],delta)]}
                for kind,value in (('grip',pose),('aim',compose(pose,aim_from_grip[hand]))):
                    live.call('set_controller_pose',{'hand':hand,'pose_type':kind,'base_space':'local',**value})
            time.sleep(.04)
        time.sleep(.20)
        return end
    def rest(hand):
        head=live.text_result(live.call('get_head_pose',{'base_space':'local'}))['pose']
        forward=rotate(head['orientation'],[0.,0.,-1.])
        yaw=math.atan2(-forward[0],-forward[2])
        upright={**head,'orientation':[0.,math.sin(yaw/2),0.,math.cos(yaw/2)]}
        pose=shifted(upright,[-.24 if hand=='left' else .24,-.48,-.25])
        pose['orientation']=mul(pose['orientation'],[.5,0.,0.,.8660254])
        move(hand,pose,.25)
    def paper_grip(hand,body,photo=False):
        # Carry the acquired paper by its actual contact, including the native
        # pinch orientation; a controller pose is not the paper's face pose.
        rendered=observe()['rendered'];current=rendered['studio']
        paper=next(p for p in current['papers'] if p['hand']==(1 if hand=='right' else 0))
        pose=paper['field_pose'] if paper['field'] else compose(current['room'],paper['pose'])
        tracking_from_native=compose(rendered['head_pose'],compose(
            {'position':[0.,0.,0.],'orientation':[0.,1.,0.,0.]},inverse_pose(rendered['native_pose'])))
        palm=compose(tracking_from_native,rendered[hand+'_palm'])
        attachment=compose(inverse_pose(palm),pose)
        return compose(body,inverse_pose(attachment))
    def hold(*channels):
        live.input([{'hand':hand,'component':component,'value':1.} for hand,component in channels],.1,lease_seconds=5.)
        time.sleep(.10)
    def camera_view(hand,head):
        # Rejoin the held camera to its actual tracked grip after the pickup
        # easing finishes; aim the lens at the cabin board rather than using
        # the controller's current orientation as a camera orientation.
        current=studio()
        grip=_controller_pose(live,hand,'grip','local')
        attachment=compose(inverse_pose(grip),current['camera'])
        target=compose(current['room'],current['board'])['position']
        camera=shifted(head,[.13 if hand=='right' else -.13,-.10,-.38])
        camera['orientation']=pointing([b-a for a,b in zip(camera['position'],target)])
        return compose(camera,inverse_pose(attachment))
    def marker_grip(body):
        rendered=observe()['rendered'];current=rendered['studio']
        side='right' if current['marker_hand']==1 else 'left'
        tracking_from_native=compose(rendered['head_pose'],compose(
            {'position':[0.,0.,0.],'orientation':[0.,1.,0.,0.]},inverse_pose(rendered['native_pose'])))
        palm=compose(tracking_from_native,rendered[side+'_palm'])
        attachment=compose(inverse_pose(palm),compose(current['room'],current['marker']))
        return compose(body,inverse_pose(attachment))
    def checkpoint(label):
        # The operator acknowledgement precedes the engine/compositor consuming
        # a changed head pose. Require a fresh native publication of that pose.
        target=live.text_result(live.call('get_head_pose',{'base_space':'local'}))['pose']
        stable_since=None;last_tracking=0;deadline=time.monotonic()+5.
        while time.monotonic()<deadline:
            rendered=observe().get('rendered') or {};actual=rendered.get('head_pose') or {}
            q=actual.get('orientation',[]);position=actual.get('position',[])
            matched=(len(q)==4 and len(position)==3
                and sum((a-b)**2 for a,b in zip(position,target['position']))<.000025
                and abs(sum(a*b for a,b in zip(q,target['orientation'])))>.9999)
            tracking=rendered.get('tracking_sequence',0)
            if matched and tracking>last_tracking:
                stable_since=stable_since or time.monotonic()
                if time.monotonic()-stable_since>.45:break
            else:stable_since=None
            last_tracking=tracking;time.sleep(.05)
        else:raise BotFault('The submitted head pose did not settle before '+label)
        if movie:
            movie.mark(label);time.sleep(1.25)
        else:captures.extend(live.capture(label,static_grips=True))
    anchor={'position':state['opening_position'],'orientation':state['opening_orientation']}
    layout=state.get('opening_bench',{}).get('layout',{})
    if not layout.get('verified'):raise BotFault('The actual native seat anchor is unavailable')
    with preserved_tracking_poses(live):
        try:
            live.release()
            seat=compose(anchor,layout['seat'])
            head=live.text_result(live.call('get_head_pose',{'base_space':'local'}))['pose']
            gaze=pointing([b-a for a,b in zip(head['position'],seat['position'])])
            live.call('set_head_pose',{'base_space':'local','position':head['position'],'orientation':gaze})
            if movie:movie.begin('cassette-insert-and-eject')
            checkpoint('native-seat-workbench')
            tape=observe()['opening_bench']['tapes'][1]
            tape_pose={'position':tape['position'],'orientation':tape['orientation']}
            home=compose(anchor,tape_pose)
            head=approach(home)
            s=observe();anchor={'position':s['opening_position'],'orientation':s['opening_orientation']}
            tape=s['opening_bench']['tapes'][1]
            home=compose(anchor,{'position':tape['position'],'orientation':tape['orientation']})
            move('right',home);hold(('right','Grip'))
            wait(lambda s:s['opening_bench']['tapes'][1]['hand']==1,'tape-picked-up')
            lifted={'position':[home['position'][0],home['position'][1]+.17,home['position'][2]],'orientation':home['orientation']}
            move('right',lifted);live.release();time.sleep(1.7)
            dropped=wait(lambda s:s['opening_bench']['tapes'][1]['hand']<0,'tape-released-and-settled')
            checkpoint('tape-on-native-seat')
            t=dropped['opening_bench']['tapes'][1]
            move('right',compose(anchor,{'position':t['position'],'orientation':t['orientation']}));hold(('right','Grip'))
            wait(lambda s:s['opening_bench']['tapes'][1]['hand']==1,'dropped-tape-recovered')
            slot=compose(anchor,layout['slot'])
            head=approach(slot)
            s=observe();anchor={'position':s['opening_position'],'orientation':s['opening_orientation']}
            layout=s['opening_bench']['layout'];slot=compose(anchor,layout['slot'])
            for height in (.12,.05,0.):move('right',shifted(slot,[0.,0.,height]));hold(('right','Grip'))
            live.release();time.sleep(.45)
            wait(lambda s:s['opening_bench']['inserted']==1,'tape-seated-without-native-import')
            rest('right')
            checkpoint('player-tape-seated')
            if movie:movie.mark('touch-eject')
            live.release()
            target=compose(anchor,layout['eject'])
            move('right',shifted(target,[0.,.08,0.]))
            for _ in range(8):
                sample=observe().get('rendered') or {}
                if not sample.get('right_index_tip_tracked'):
                    raise BotFault('Rendered index fingertip is unavailable for physical eject')
                tip=sample['right_index_tip_local']['position']
                delta=[a-b for a,b in zip(target['position'],tip)]
                if math.dist(tip,target['position'])<.006:break
                grip=_controller_pose(live,'right','grip','local')
                move('right',{**grip,'position':[a+b for a,b in zip(grip['position'],delta)]},seconds=.2)
            else:raise BotFault('Index fingertip could not reach the physical eject button')
            time.sleep(.50)
            ejected=wait(lambda s:s['opening_bench']['inserted']==-1,'tape-ejected')
            presented=wait(lambda s:s['opening_bench']['tapes'][1]['state']==6,'ejected-tape-supported-in-carrier')
            position=presented['opening_bench']['tapes'][1]['position']
            time.sleep(.75)
            retained=observe()['opening_bench']['tapes'][1]
            if retained['state']!=6 or math.dist(position,retained['position'])>.002:
                raise BotFault('Ejected cassette fell or drifted out of its carrier')
            rest('right');checkpoint('player-lid-open-cassette-presented')
            if movie:movie.mark('tape-ejected');time.sleep(1.)
            t=retained
            pickup=compose(anchor,{'position':t['position'],'orientation':t['orientation']})
            move('right',{**pickup,'position':[pickup['position'][0],pickup['position'][1]+.15,pickup['position'][2]]})
            hold(('right','Grip'))
            move('right',pickup)
            wait(lambda s:s['opening_bench']['tapes'][1]['hand']==1,'ejected-tape-recovered')
            move('right',{'position':[home['position'][0],home['position'][1]+.06,home['position'][2]],'orientation':home['orientation']})
            live.release();time.sleep(.40)
            if movie:movie.begin('instant-camera-and-polaroid')
            camera=studio()['camera'];head=approach(camera)
            camera=studio()['camera']
            move('right',shifted(camera,[.05,-.025,0.]));hold(('right','Grip'))
            wait(lambda s:s['rendered']['studio']['camera_hand']==1,'physical-camera-grabbed')
            if movie:time.sleep(.4);hold(('right','Grip'))
            camera_target=camera_view('right',head) if movie else shifted(head,[.16,-.10,-.42])
            if movie:movie.mark('camera-shutter')
            move('right',camera_target);hold(('right','Grip'),('right','Trigger'))
            wait(lambda s:s['rendered']['studio']['photo']>0,'camera-native-scene-printed',seconds=8.)
            live.release(keep=(('right','Grip',None),));rest('left');checkpoint('camera-live-view-and-print')
            grip=_controller_pose(live,'right','grip','local')
            camera=studio()['camera']
            attachment=compose(inverse_pose(grip),camera)
            front={**camera,'orientation':mul(camera['orientation'],[0.,1.,0.,0.])}
            move('right',compose(front,inverse_pose(attachment)));hold(('right','Grip'))
            checkpoint('instant-camera-front-and-side-grip')
            move('right',grip);hold(('right','Grip'))
            room=studio()['room'];move('right',compose(room,studio()['camera_home']))
            live.release();time.sleep(.30)
            rest('right')
            camera=studio()['camera'];head=approach(camera)
            # Keep the hand open while it travels across the table: a loose
            # print can otherwise be picked up before the hand reaches the camera.
            live.release()
            move('left',studio()['camera'])
            hold(('left','Grip'))
            picked=studio()
            if any(p['hand']==0 for p in picked['papers']) or picked['marker_hand']==0:
                raise BotFault('Left hand acquired another object at camera contact')
            wait(lambda s:s['rendered']['studio']['camera_hand']==0,'left-camera-grip-before-contact')
            previous_photo=studio()['photo']
            if movie:time.sleep(.4);hold(('left','Grip'))
            move('left',camera_view('left',head) if movie else shifted(head,[-.16,-.10,-.42]))
            hold(('left','Grip'),('left','Trigger'))
            printed=wait(lambda s:s['rendered']['studio']['photo']>previous_photo,'left-camera-native-scene-printed',seconds=8.)
            printed_photo=printed['rendered']['studio']['photo']
            live.release(keep=(('left','Grip',None),))
            checkpoint('left-camera-live-view-and-print')
            room=studio()['room'];move('left',compose(room,studio()['camera_home']))
            live.release();time.sleep(.30);rest('left')
            if not movie:
                current=studio()
                existing_note_ids={p['id'] for p in current['papers'] if not p['photo']}
                if any(not p['photo'] and not p['pinned'] for p in current['papers']):
                    raise BotFault('Workbench note test would rewrite an existing note; collection preserved')
                note_home=compose(room,current['notes_home'])
                head=approach(note_home)
                # Moving the observer can republish the cabin in a new pose.
                # Rejoin the notepad to the fresh room.
                current=studio();room=current['room']
                note_home=compose(room,current['notes_home'])
                hold(('right','Grip'))
                move('right',note_home)
                # Persistent loose prints can cover the notepad after earlier
                # physical runs. Move an encountered print aside through the same
                # grip/release path; never delete or rewrite the collection.
                for _ in range(3):
                    acquired=wait(lambda s:any(p['hand']==1 for p in s['rendered']['studio']['papers']),
                                  'notepad-contact-acquired')
                    held_paper=next(p for p in acquired['rendered']['studio']['papers'] if p['hand']==1)
                    if not held_paper['photo']:break
                    move('right',compose(studio()['room'],{'position':[.20,-2.0,-1.10],'orientation':[0.,0.,0.,1.]}))
                    live.release();time.sleep(.25)
                    current=studio()
                    move('right',compose(current['room'],current['notes_home']))
                    hold(('right','Grip'))
                right_note=wait(lambda s:any(p['hand']==1 and not p['photo'] for p in s['rendered']['studio']['papers']),
                                'right-sticky-note-grip-before-contact')
                right_note_id=next(p['id'] for p in right_note['rendered']['studio']['papers'] if p['hand']==1 and not p['photo'])
                if right_note_id in existing_note_ids:
                    raise BotFault('Workbench acquired an existing note; refusing to rewrite it')
                move('right',paper_grip('right',shifted(head,[.12,-.18,-.45])))
                checkpoint('right-hand-sticky-note')
                move('right',paper_grip('right',note_home))
                live.release()
                wait(lambda s:any(p['id']==right_note_id and p['hand']<0 for p in s['rendered']['studio']['papers']),
                     'right-sticky-note-released')
                rest('right')
                current=studio();note_home=compose(current['room'],next(p for p in current['papers'] if p['id']==right_note_id)['pose'])
                head=approach(note_home)
                current=studio();room=current['room'];note_home=compose(room,next(p for p in current['papers'] if p['id']==right_note_id)['pose'])
                move('left',note_home);hold(('left','Grip'))
                picked=wait(lambda s:any(p['hand']==0 and not p['photo'] for p in s['rendered']['studio']['papers']),'sticky-note-peeled')
                note=next(p for p in picked['rendered']['studio']['papers'] if p['hand']==0 and not p['photo'])
                note_id=note['id']
                if note_id!=right_note_id or note_id in existing_note_ids:
                    raise BotFault('Workbench did not reacquire its own new note; refusing to rewrite it')
                note_pose=shifted(head,[-.10,-.18,-.45])
                move('left',paper_grip('left',note_pose));hold(('left','Grip'))
                aim_at('right',shifted(note_pose,[0.,0.,.25]));hold(('left','Grip'),('right','Trigger'))
                wait(lambda s:s['rendered']['studio']['editor']==note_id,'vr-note-keyboard-opened')
                live.release(keep=(('left','Grip',None),))
                keys=['Q','W','E','R','T','Y','U','I','O','P','7','A','S','D','F','G','H','J','K','L','8','9',
                      'Z','X','C','V','B','N','M','.',',','5','6','SPACE','BACK','CLEAR','DONE','0','1','2','3','4','-','TRASH']
                for letter in ('CLEAR','T','A','P','E','DONE'):
                    current=studio();key=keys.index(letter)
                    target=compose(current['room'],shifted(current['keyboard'],[(key%11-5)*.045,(.5-key//11)*.045,.25]))
                    aim_at('right',target,.15);hold(('left','Grip'),('right','Trigger'))
                    live.release(keep=(('left','Grip',None),));time.sleep(.08)
                typed=wait(lambda s:any(p['id']==note_id and p['text']=='TAPE' for p in s['rendered']['studio']['papers']),'sharpie-note-caption')
                marker_pose=compose(room,typed['rendered']['studio']['marker'])
                move('right',marker_pose);hold(('left','Grip'),('right','Grip'))
                wait(lambda s:s['rendered']['studio']['marker_hand']==1,'physical-marker-grabbed')
                for dx in (-.020,-.010,0.,.010,.020):
                    move('right',marker_grip(shifted(note_pose,[dx,-.025,.08])),.12)
                    hold(('left','Grip'),('right','Grip'),('right','Trigger'))
                live.release(keep=(('left','Grip',None),('right','Grip',None)))
                wait(lambda s:any(p['id']==note_id and p['strokes']>0 for p in s['rendered']['studio']['papers']),'freehand-marker-stroke')
                checkpoint('marker-tip-and-grip-writing-pose')
                move('right',marker_grip(compose(room,studio()['marker_home'])))
                live.release(keep=(('left','Grip',None),))
                board=studio()['board']
                board_note=compose(room,shifted(board,[-.09,0.,.006]))
                board_world=compose(room,board)
                normal=rotate(board_world['orientation'],[0.,0.,1.])
                head=approach(board_world,[n*.55+( .10 if i==1 else 0.) for i,n in enumerate(normal)])
                room=studio()['room'];board_world=compose(room,board)
                board_note=compose(room,shifted(board,[-.09,0.,.006]))
                gaze=pointing([b-a for a,b in zip(head['position'],board_world['position'])])
                live.call('set_head_pose',{'base_space':'local','position':head['position'],'orientation':gaze})
                rest('right')
                hold(('left','Grip'));move('left',paper_grip('left',compose(room,shifted(board,[-.09,0.,.065]))))
                checkpoint('note-held-in-front-of-native-board')
                hold(('left','Grip'));move('left',paper_grip('left',board_note));live.release();time.sleep(.12)
                wait(lambda s:any(p['id']==note_id and p['pinned'] for p in s['rendered']['studio']['papers']),'note-attached-to-native-board')
            else:
                movie.begin('attach-photo-to-wall')
                current=studio();room=current['room'];board=current['board']
                board_world=compose(room,board)
                normal=rotate(board_world['orientation'],[0.,0.,1.])
            photo=next((p for p in studio()['papers'] if p['photo']==printed_photo),None)
            if photo is None:raise BotFault('The photo just printed is no longer available for placement')
            head=approach(compose(room,photo['pose']))
            room=studio()['room']
            photo=next(p for p in studio()['papers'] if p['id']==photo['id'])
            move('left',compose(room,photo['pose']));hold(('left','Grip'))
            wait(lambda s:any(p['id']==photo['id'] and p['hand']==0 for p in s['rendered']['studio']['papers']),'polaroid-picked-up')
            if movie:movie.mark('polaroid-in-hand')
            head=approach(board_world,[n*.55+( .10 if i==1 else 0.) for i,n in enumerate(normal)])
            room=studio()['room'];board_world=compose(room,board)
            # Avoid silently placing a new print on top of an existing paper.
            # Board and paper poses are both room-local here.
            occupied=[]
            for paper in studio()['papers']:
                if paper['pinned']:
                    local=compose(inverse_pose(board),paper['pose'])['position']
                    half=(.044,.0535) if paper['photo'] else (.038,.038)
                    occupied.append((local,half))
            locations=((.09,0.),(0.,.14),(0.,-.14),(-.13,.14),(.13,.14),(-.13,-.14),(.13,-.14),(-.09,0.))
            placement=next(((x,y) for x,y in locations if all(
                abs(x-p[0])>=.044+h[0]+.005 or abs(y-p[1])>=.0535+h[1]+.005
                for p,h in occupied)),None)
            if placement is None:raise BotFault('No clear board space for the printed photo')
            hold(('left','Grip'));move('left',paper_grip('left',compose(room,shifted(board,[*placement,.006])),True))
            live.release();time.sleep(.12)
            wait(lambda s:any(p['id']==photo['id'] and p['pinned'] for p in s['rendered']['studio']['papers']),'polaroid-attached-to-native-board')
            board_world=compose(room,board)
            gaze=pointing([b-a for a,b in zip(head['position'],board_world['position'])])
            live.call('set_head_pose',{'base_space':'local','position':head['position'],'orientation':gaze})
            rest('left');rest('right')
            checkpoint('native-board-polaroid' if movie else 'native-board-polaroid-and-sharpie-note')
            if not movie:
                current=studio();marker_world=compose(current['room'],current['marker'])
                approach(marker_world);current=studio()
                move('left',compose(current['room'],current['marker']));hold(('left','Grip'))
                wait(lambda s:s['rendered']['studio']['marker_hand']==0,'left-marker-grabbed')
                current=studio();paper=next(p for p in current['papers'] if p['id']==note_id)
                paper_world=compose(current['room'],paper['pose']);normal=rotate(paper_world['orientation'],[0.,0.,1.])
                approach(paper_world,[n*.55+(.10 if i==1 else 0.) for i,n in enumerate(normal)])
                current=studio();paper=next(p for p in current['papers'] if p['id']==note_id)
                stroke_count=paper['strokes'];paper_world=compose(current['room'],paper['pose'])
                for dx in (-.020,-.010,0.,.010,.020):
                    move('left',marker_grip(shifted(paper_world,[dx,.01,.08])),.12)
                    hold(('left','Grip'),('left','Trigger'))
                live.release(keep=(('left','Grip',None),))
                wait(lambda s:any(p['id']==note_id and p['strokes']>stroke_count for p in s['rendered']['studio']['papers']),
                     'left-marker-board-stroke')
                checkpoint('left-marker-tip-and-grip-writing-pose')
                current=studio();approach(compose(current['room'],current['marker_home']))
                move('left',marker_grip(compose(studio()['room'],studio()['marker_home'])));live.release()
            if movie:movie.end()
            return {'status':'observed_workbench_loop','outcomes':outcomes,'captures':captures,
                    'film':movie.takes if movie else None,
                    'elapsed_seconds':time.monotonic()-started,'visual_acceptance':'pending_both_eye_review'}
        finally:
            live.release()
            if movie:movie.end()


def inspect_persisted_board(live, timeout=90.):
    """Read and view the saved physical board after a fresh game launch."""
    state=wait_for_continue_rack(live,advance_startup(live,timeout=timeout))
    ready_until=time.monotonic()+30.
    while not ((state.get('rendered') or {}).get('studio',{}).get('available')
            and (state.get('rendered') or {}).get('studio',{}).get('cabin')):
        if time.monotonic()>ready_until or state.get('scene')!='title':
            raise BotFault('The restarted native cabin did not finish becoming ready')
        time.sleep(.1);state=live.observe()
    saved=(state.get('rendered') or {}).get('studio') or {}
    papers=[p for p in saved.get('papers',[]) if p['pinned']]
    if not saved.get('cabin') or len(papers)<2:
        raise BotFault('The restarted cabin did not restore its note and Polaroid')
    with preserved_tracking_poses(live):
        try:
            live.release()
            for _ in range(2):
                studio=live.observe()['rendered']['studio']
                board=compose(studio['room'],studio['board'])
                normal=rotate(board['orientation'],[0.,0.,1.])
                position=[p+n*.65+(.12 if i==1 else 0.) for i,(p,n) in enumerate(zip(board['position'],normal))]
                head={'position':position,'orientation':pointing([a-b for a,b in zip(board['position'],position)])}
                live.call('set_head_pose',{'base_space':'local',**head,'duration_seconds':.5})
                time.sleep(.75)
            for hand,side in (('left',-1),('right',1)):
                grip=shifted(head,[side*.24,-.45,-.20])
                for kind in ('grip','aim'):
                    live.call('set_controller_pose',{'hand':hand,'pose_type':kind,'base_space':'local',**grip})
            time.sleep(.75)
            state=live.observe();current=state['rendered']['studio']
            restored=[p for p in current['papers'] if p['pinned']]
            if restored!=papers or current['camera_hand']>=0 or current['marker_hand']>=0:
                raise BotFault('The restored board changed during its read-only viewing check')
            return {'status':'observed_persisted_board','papers':restored,
                    'captures':live.capture('restored-native-board',static_grips=True),
                    'visual_acceptance':'pending_both_eye_review'}
        finally:
            live.release()
