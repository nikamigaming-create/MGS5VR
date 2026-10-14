"""Navigation contracts: floors, explicit portals, obstacles and native stance."""
import math
import copy
import json
import pathlib
import struct
import sys
import tempfile
import unittest
import hashlib
import heapq
import random
from unittest.mock import Mock, patch

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]/'tools'))
from gameplay_bot.core import BattleResponseNeeded, BotFault
from gameplay_bot.navigation import NativeMap, NavigationAtlas, NoConnectedNativeRoute, ObstacleMemory, RouteProgress, _ThreatCosts, arrived, movement_mode, read_nav2
from gameplay_bot.route import finish_navigation, navigate, set_posture
from gameplay_bot.perception import read_enemy_snapshot
from gameplay_bot.hospital_navigation import HospitalEventRoute


def tile(name, points, edges, *, boundaries=(), cross=(), group=4):
    return {'source':name, 'sha256':name*64, 'groups':[{'id':group,'nodes':points,
        'edges':[{'from':a,'to':b,'flags':1249} for a,b in edges],
        'boundaries':[{'node':i} for i in boundaries], 'cross_group':list(cross)}]}


class NavigationTests(unittest.TestCase):
    def hospital_manifest(self, directory, center):
        directory=pathlib.Path(directory)
        source=directory/'owned-source';source.write_bytes(b'owned-fixture')
        row={'file':str(source),'sha256':hashlib.sha256(source.read_bytes()).hexdigest()}
        manifest=directory/'events.json'
        manifest.write_text(json.dumps({'mission':10010,'location':30,'story':0,
            'scene':row,'tiles':[row],'events':{'trap':{'sequence':'Seq_Game_Test',
            'center':center,'size':[1.,3.,2.],'support_y':0.}}}))
        native={'mission':10010,'location':30,'story':0,'sequence':'Seq_Game_Test',
                'player_x':0.,'player_y':.9,'player_z':0.,'status_SQUAT':True,
                'status_CRAWL':False,'status_STAND':False}
        return manifest,native

    def test_hospital_sparse_event_uses_floor_topology_instead_of_raised_body_node(self):
        nav=NativeMap([tile('a',[(0,0,0),(3,0,0),(6,0,0),(0,.9,0)],[(0,1),(1,2)])])
        with tempfile.TemporaryDirectory() as directory:
            manifest,native=self.hospital_manifest(directory,[4.75,1.,0.])
            self.assertEqual(nav.nearest([0,.9,0]),3)
            with patch.object(NativeMap,'from_files',return_value=nav):
                route=HospitalEventRoute(str(manifest),'trap','Seq_Game_Test',native)
            self.assertEqual(route.route,[0,1,2])
            self.assertEqual(route.target([0,.9,0]),(3,0,0))
            self.assertTrue(route.inside([4.5,.9,0]))

    def test_hospital_obstruction_is_remembered_and_changes_next_route(self):
        nav=NativeMap([tile('a',[(0,0,0),(3,0,0),(0,0,2),(6,0,0)],
                           [(0,1),(1,3),(0,2),(2,3)])])
        with tempfile.TemporaryDirectory() as directory:
            manifest,native=self.hospital_manifest(directory,[4.75,1.,0.])
            with patch.object(NativeMap,'from_files',return_value=nav):
                route=HospitalEventRoute(str(manifest),'trap','Seq_Game_Test',native)
                self.assertEqual(route.route,[0,1,3])
                route.index=1
                self.assertEqual(route.obstructed([0,.9,0]),(0,1))
                self.assertEqual(route.route,[0,2,3])
                resumed=HospitalEventRoute(str(manifest),'trap','Seq_Game_Test',native)
                self.assertEqual(resumed.route,[0,2,3])

    def test_hospital_waypoint_uses_current_posture_floor_offset(self):
        nav=NativeMap([tile('a',[(0,0,0),(3,0,0),(6,0,0)],[(0,1),(1,2)])])
        with tempfile.TemporaryDirectory() as directory:
            manifest,native=self.hospital_manifest(directory,[4.75,1.,0.])
            data=json.loads(manifest.read_text())
            data['events']['trap']['root_to_floor_offsets']={'SQUAT':.9,'CRAWL':.425}
            manifest.write_text(json.dumps(data))
            with patch.object(NativeMap,'from_files',return_value=nav):
                route=HospitalEventRoute(str(manifest),'trap','Seq_Game_Test',native)
            route.target([0,.9,0])
            route.observe_posture({'status_CRAWL':True,'status_SQUAT':False,'status_STAND':False})
            self.assertEqual(route.target([3,.425,0]),(6,0,0))

    def test_disconnected_directed_topology_refused_before_weighted_search(self):
        nav=NativeMap([tile('a',[(0,0,0),(1,0,0),(2,0,0)],[(0,1)])])
        with patch('gameplay_bot.navigation._ThreatCosts.penalty',side_effect=AssertionError('weighted search ran')):
            with self.assertRaises(NoConnectedNativeRoute): nav.path(0,2,threats=[(0,0,0)]*20)
            with self.assertRaises(NoConnectedNativeRoute): nav.path(1,0)
        self.assertEqual(nav.path(0,1),[0,1])

    def test_reachability_cache_is_bounded_and_obstacles_cannot_authorize_edges(self):
        nav=NativeMap([tile('a',[(0,0,0),(1,0,0),(2,0,0),(3,0,0)],[(0,1),(1,2),(2,3)])])
        self.assertEqual(nav.path(0,3),[0,1,2,3])
        self.assertEqual(nav.path(1,3),[1,2,3])
        self.assertEqual(nav.path(2,3),[2,3])
        self.assertEqual(list(nav._reachability),[1,2])
        self.assertTrue(all(isinstance(bits,bytearray) and len(bits)==4 for bits in nav._reachability.values()))
        with self.assertRaises(NoConnectedNativeRoute): nav.path(2,3,blocked=[(2,3)])
        self.assertEqual(nav.path(2,3),[2,3])

    def test_cancelled_reachability_is_not_cached_and_cache_hit_still_checks_owner(self):
        nav=NativeMap([tile('a',[(0,0,0),(1,0,0)],[(0,1)])]);calls=[]
        def cancel():
            calls.append(True)
            if len(calls)==2: raise BattleResponseNeeded('changed during connectivity')
        with patch('gameplay_bot.navigation.time.monotonic',side_effect=[0.,.2]), self.assertRaises(BattleResponseNeeded):
            nav.path(0,1,check_abort=cancel)
        self.assertFalse(nav._reachability)
        self.assertEqual(nav.path(0,1),[0,1]);calls=[]
        with self.assertRaises(BattleResponseNeeded): nav.path(0,1,check_abort=cancel)
        self.assertEqual(len(calls),2)

    def test_spatial_threat_cost_is_exact_at_negative_cells_radius_and_height_bounds(self):
        randomizer=random.Random(716)
        threats=[(65.,0.,0.),(-65.,0.,0.),(0.,12.,0.),(0.,11.999999,0.),(0.,0.,0.)]*4
        threats += [(randomizer.uniform(-400,400),randomizer.uniform(-30,30),
                     randomizer.uniform(-400,400)) for _ in range(80)]
        field=_ThreatCosts(threats)
        points=[(x,y,z) for x in (-130.,-65.,-0.000001,0.,64.999999,65.,130.)
                for y in (0.,12.,24.) for z in (-65.,0.,65.)]
        for point in points:
            expected=sum(max(0.,1.-math.dist(point,threat)/65.)**2*12.
                         for threat in threats if abs(point[1]-threat[1])<12.)
            self.assertEqual(field.penalty(point),expected)

    def test_far_threats_do_not_require_distances_and_duplicate_local_cost_is_retained(self):
        threats=[(2000.+i,0.,2000.) for i in range(100)]+[(10.,0.,0.)]*2
        field=_ThreatCosts(threats)
        with patch('gameplay_bot.navigation.math.dist',wraps=math.dist) as distance:
            actual=field.penalty((0.,0.,0.))
        self.assertEqual(distance.call_count,2)
        self.assertEqual(actual,2*(1.-10./65.)**2*12.)

    def test_optimized_paths_match_original_weighted_astar_with_direction_and_obstacles(self):
        def original(nav,start,goal,blocked,threats):
            queue=[(math.dist(nav.positions[start],nav.positions[goal]),0.,start)]
            costs={start:0.};previous={};risk={}
            while queue:
                _,cost,node=heapq.heappop(queue)
                if cost!=costs[node]:continue
                if node==goal:
                    route=[node]
                    while node in previous:node=previous[node];route.append(node)
                    return list(reversed(route))
                for target,(length,_) in nav.adj[node].items():
                    if (node,target) in blocked:continue
                    if target not in risk:
                        p=nav.positions[target]
                        risk[target]=sum(max(0.,1.-math.dist(p,t)/65.)**2*12.
                                        for t in threats if abs(p[1]-t[1])<12.)
                    candidate=cost+length*(1.+risk[target])
                    if candidate<costs.get(target,math.inf):
                        costs[target]=candidate;previous[target]=node
                        heapq.heappush(queue,(candidate+math.dist(nav.positions[target],nav.positions[goal]),candidate,target))
            return None
        randomizer=random.Random(9404)
        points=[(x*25.,0.,z*25.) for x in range(4) for z in range(4)]
        edges=[(a,b) for a in range(16) for b in range(16) if a!=b and math.dist(points[a],points[b])<=35.5
               and randomizer.random()<.75]
        nav=NativeMap([tile('a',points,edges)])
        threats=[(randomizer.uniform(-200,200),randomizer.uniform(-14,14),randomizer.uniform(-200,200))
                 for _ in range(60)]
        for start,goal in ((0,15),(15,0),(2,12),(5,10),(7,14)):
            for blocked in (set(),set(edges[::7])):
                expected=original(nav,start,goal,blocked,threats)
                if expected is None:
                    with self.assertRaises(NoConnectedNativeRoute): nav.path(start,goal,blocked=blocked,threats=threats)
                else:self.assertEqual(nav.path(start,goal,blocked=blocked,threats=threats),expected)

    def test_detection_preempts_navigation_before_route_search_or_motion(self):
        live=Mock();live.bindings={'axes':[{'name':'axes.move','source':'left_stick'}],
            'actions':[{'name':'gameplay.run','bindings':[{'inputs':['left_stick_click']}]}]}
        live.text_result.return_value={'pose':{'position':[0,0,0],'orientation':[0,0,0,1]}}
        state={'native':{'mission':30010,'location':10,'player_x':0,'player_y':0,'player_z':0,
                         'player_life':7801,'not_alert':False}}
        live.observe.return_value=state;nav=Mock(identity='owned-map');reader=Mock()
        with tempfile.TemporaryDirectory() as directory, \
             patch('gameplay_bot.combat.fresh_rig',return_value=(state,(30010,10,1,2,3))):
            result=navigate(live,nav,[10,0,0],output=pathlib.Path(directory),enemy_reader=reader,
                            recenter=False,interrupt_on_danger=True)
        self.assertEqual(result['status'],'needs_battle_response');self.assertEqual(result['reason'],'detected')
        live.input.assert_not_called();live.execute.assert_not_called();live.capture.assert_not_called()
        reader.assert_not_called();nav.path.assert_not_called()

    def test_vault_recovery_releases_movement_and_rechecks_world_before_arrival(self):
        foot={'scene':'gameplay','camera_active':True,'camera_available':True,
              'menu':False,'idroid':False,'gamepad':False,'now_ms':100,
              'controls':{'context':'gameplay','rig_input':True,'travel_mode':1,
                          'age_ms':0,'sample_ms':100},
              'native':{'mission':30010,'location':10,'player_x':0,'player_y':0,'player_z':0,
                        'player_life':7801,'not_alert':True,'player_vehicle_id':65535,
                        'popup':False,'saving':False,'demo':False,'status_CARRY':False,
                        'status_NORMAL_ACTION':True,'status_STAND':True}}
        vault=copy.deepcopy(foot)
        vault['controls']['travel_mode']=4;vault['native']['status_NORMAL_ACTION']=False
        changed=copy.deepcopy(vault);changed['native']['location']=20
        for recovery,expected in ((vault,'observed_native_route'),(changed,'failed')):
            with self.subTest(recovery_location=recovery['native']['location']):
                live=Mock();live.bindings={'axes':[{'name':'axes.move','source':'left_stick'}],
                    'actions':[{'name':'gameplay.run','bindings':[{'inputs':['left_stick_click']}]}]}
                live.text_result.return_value={'pose':{'position':[0,0,0],'orientation':[0,0,0,1]}}
                live.observe.return_value=foot;live.capture.return_value=[]
                nav=Mock(identity='owned-map')
                reader=Mock(return_value={'mission':30010,'location':10,'truncated':False,
                    'unavailable_loaded':0,'normal_life':'0','soldiers':[]})
                samples=[foot,vault,recovery,foot,foot]
                with tempfile.TemporaryDirectory() as directory, \
                     patch('gameplay_bot.combat.fresh_rig',side_effect=[(s,(30010,10,1,2,3)) for s in samples]):
                    result=navigate(live,nav,[0,0,0],output=pathlib.Path(directory),enemy_reader=reader,
                                    recenter=False,max_seconds=1.,sleep=lambda _:None)
                self.assertEqual(result['status'],expected)
                if expected=='failed':self.assertIn('owner changed',result['error'])
                self.assertGreaterEqual(live.release.call_count,3)
                live.input.assert_not_called();live.execute.assert_not_called();nav.path.assert_not_called()

    def test_route_search_can_be_preempted_by_fresh_battle_telemetry(self):
        nav=NativeMap([tile('a',[(0,0,0),(10,0,0)],[(0,1)])]);calls=[]
        def check():
            calls.append(True)
            if len(calls)>1:raise BattleResponseNeeded('detected while planning')
        with patch('gameplay_bot.navigation.time.monotonic',side_effect=[0.,.2]), \
             self.assertRaisesRegex(BattleResponseNeeded,'while planning'):
            nav.path(0,1,check_abort=check)
        self.assertEqual(len(calls),2)

    def test_stealth_policy_stays_low_before_detection_and_evades_real_attack(self):
        native={'player_x':0,'player_y':0,'player_z':0,'player_life':7801,'not_alert':True}
        self.assertEqual(movement_mode(native,[[150,0,0]],stealth=True),'run')
        self.assertEqual(movement_mode(native,[[89,0,0]],stealth=True),'crouch')
        self.assertEqual(movement_mode(native,[[95,0,0]],previous_mode='crouch',stealth=True),'crouch')
        self.assertEqual(movement_mode(native,[[101,0,0]],previous_mode='crouch',stealth=True),'run')
        self.assertEqual(movement_mode(native,[[54,0,0]],stealth=True),'crawl')
        self.assertEqual(movement_mode(native,[[58,0,0]],previous_mode='crawl',stealth=True),'crawl')
        self.assertEqual(movement_mode({**native,'not_alert':False},[[30,0,0]],stealth=True),'evade')
        self.assertEqual(movement_mode({**native,'player_life':7330},[],previous_life=7801,stealth=True),'retreat')

    def test_precise_arrival_radius_cannot_relax_floor_or_be_unbounded(self):
        self.assertTrue(arrived((0,1,1),(0,0,0),radius=1.5))
        self.assertFalse(arrived((0,3,1),(0,0,0),radius=1.5))
        for radius in (False,0.,10.,float('nan')):
            with self.subTest(radius=radius),self.assertRaisesRegex(BotFault,'Arrival radius'):
                navigate(None,None,[0,0,0],output=None,enemy_reader=None,arrival_radius=radius)

    def enemy_pages(self):
        row=lambda name,id: {'name':name,'id':id,'life':'0','position':[id,2.,3.]}
        common={'mission':30010,'location':10,'definition_identity':1234,'definitions_total':3,
                'normal_life':'0','truncated':False,'unavailable_loaded':0}
        return [{**common,'page_cursor':0,'next_cursor':2,'definitions_inspected':2,
                 'unloaded_count':1,'soldiers':[row('a',1)]},
                {**common,'page_cursor':2,'next_cursor':None,'definitions_inspected':1,
                 'unloaded_count':0,'soldiers':[row('b',2)]}]

    def test_enemy_pages_join_all_definitions_before_perception_is_admitted(self):
        pages=self.enemy_pages();scripts=[]
        def read(script):
            scripts.append(script);return pages.pop(0),{'seconds':.02}
        result=read_enemy_snapshot(read,'return native_query',clock=lambda:0.)
        self.assertEqual(result['definitions_inspected'],3)
        self.assertEqual(result['unloaded_count'],1)
        self.assertEqual([r['name'] for r in result['soldiers']],['a','b'])
        self.assertEqual(len(result['pages']),2)
        self.assertTrue(scripts[1].startswith('local MGS5VR_NAV_QUERY_CURSOR=2\n'))

    def test_enemy_pages_reject_incomplete_or_changed_world_without_accepting_partial_rows(self):
        for edit in ({'truncated':True},{'unavailable_loaded':1},{'location':20},
                     {'definition_identity':1235},{'next_cursor':2}):
            pages=self.enemy_pages();pages[1].update(edit)
            with self.subTest(edit=edit),self.assertRaises(BotFault):
                read_enemy_snapshot(lambda _: (pages.pop(0),{}),'query',clock=lambda:0.)

    def test_enemy_pages_reject_cursor_gaps_duplicate_actors_and_nonfinite_positions(self):
        for edit in ({'page_cursor':1},{'definitions_inspected':0},
                     {'soldiers':[{'name':'a','id':2,'life':'0','position':[1,2,3]}]},
                     {'soldiers':[{'name':'b','id':2,'life':'0','position':[float('nan'),2,3]}]}):
            pages=self.enemy_pages();pages[1].update(edit)
            with self.subTest(edit=edit),self.assertRaises(BotFault):
                read_enemy_snapshot(lambda _: (pages.pop(0),{}),'query',clock=lambda:0.)

    def test_enemy_pages_stop_at_the_snapshot_deadline(self):
        pages=self.enemy_pages();times=iter([0.,0.,4.])
        with self.assertRaisesRegex(BotFault,'bounded read deadline'):
            read_enemy_snapshot(lambda _: (pages.pop(0),{}),'query',clock=lambda:next(times))
        self.assertEqual(len(pages),1)

    def test_atlas_loads_each_world_separately_even_with_one_combined_manifest(self):
        with tempfile.TemporaryDirectory() as d:
            manifest=[]
            for key in ['afgh','mafr']:
                path=pathlib.Path(d)/(key+'.nav2');path.write_bytes(key.encode())
                manifest.append({'path':f'/Assets/tpp/level/location/{key}/test.nav2','file':str(path),
                                 'sha256':hashlib.sha256(path.read_bytes()).hexdigest()})
            atlas=NavigationAtlas(manifest,{'locations':[{'key':'afgh','code':10},{'key':'mafr','code':20}]})
            with patch.object(NativeMap,'from_files') as loader:
                atlas.select(20)
                self.assertEqual(loader.call_args.args[0],[str((pathlib.Path(d)/'mafr.nav2').resolve())])
                atlas.select(10)
                self.assertEqual(loader.call_args.args[0],[str((pathlib.Path(d)/'afgh.nav2').resolve())])

    def test_atlas_cannot_reuse_an_afghanistan_graph_in_africa(self):
        with tempfile.TemporaryDirectory() as d:
            path=pathlib.Path(d)/'owned.nav2';path.write_bytes(b'owned bytes')
            worlds={'locations':[{'key':'afgh','code':10},{'key':'mafr','code':20}]}
            manifest=[{'path':'/Assets/tpp/level/location/afgh/test.nav2','file':str(path),
                       'sha256':hashlib.sha256(path.read_bytes()).hexdigest()}]
            atlas=NavigationAtlas(manifest,worlds)
            with patch.object(NativeMap,'from_files',return_value='loaded') as loader:
                with self.assertRaisesRegex(BotFault,'No imported navigation'):
                    atlas.select(20)
                loader.assert_not_called()
                self.assertEqual(atlas.select(10),'loaded')
            self.assertEqual(atlas.summary()['locations'][1]['coverage'],'not_imported')
            self.assertFalse(atlas.summary()['full_map_acceptance'])

    def test_atlas_refuses_changed_assets_and_unknown_location_paths(self):
        with tempfile.TemporaryDirectory() as d:
            path=pathlib.Path(d)/'owned.nav2';path.write_bytes(b'changed')
            worlds={'locations':[{'key':'afgh','code':10}]}
            manifest=[{'path':'/Assets/tpp/level/location/afgh/test.nav2','file':str(path),'sha256':'a'*64},
                      {'path':'/Assets/tpp/level/location/unknown/test.nav2','file':str(path),'sha256':'a'*64}]
            atlas=NavigationAtlas(manifest,worlds)
            self.assertEqual(atlas.summary()['unresolved_tiles'],1)
            with self.assertRaisesRegex(BotFault,'changed or is missing'):
                atlas.select(10)
            with self.assertRaises(BotFault): atlas.select(True)

    def test_location_change_after_atlas_selection_dispatches_no_navigation_input(self):
        class Live:
            calls=[]
            def observe(self,**_): return {'native':{'location':20}}
            def call(self,*_): self.calls.append('input');raise AssertionError('No pose/input dispatch')
        live=Live()
        with self.assertRaisesRegex(BotFault,'no movement dispatched'):
            navigate(live,None,[0,0,0],output=None,enemy_reader=None,expected_location=10)
        self.assertEqual(live.calls,[])

    def test_arrival_accepts_observed_slope_offset_without_flattening_floors(self):
        # Actual Mission 6 road versus its authored NPC navigation vertex.
        p=(2100.11035,349.22467,-73.37537)
        target=(2099.89824,347.45001,-73.45010)
        self.assertTrue(arrived(p,target))
        self.assertFalse(arrived((p[0],target[1]+3,p[2]),target))
        self.assertFalse(arrived((p[0],target[1]+20,p[2]),target))
        self.assertFalse(arrived((target[0]+2,target[1],target[2]),target))

    def test_lateral_jitter_cannot_keep_stalled_edge_alive(self):
        progress=RouteProgress()
        self.assertFalse(progress.stalled(1,5.,0.))
        self.assertFalse(progress.stalled(1,4.9,1.))
        self.assertTrue(progress.stalled(1,5.1,2.1))
        self.assertFalse(progress.stalled(1,4.5,2.2))
        self.assertFalse(progress.stalled(2,6.,5.))

    def test_transport_failure_keeps_result_and_native_observation(self):
        class Live:
            def release(self): raise BotFault('transport failed')
            def call(self, *_): raise BotFault('transport failed')
            def observe(self, **_): return {'native': {'player_life': 7801}}
            def capture(self, *_): raise AssertionError('Do not capture while release is unconfirmed')
        with tempfile.TemporaryDirectory() as d:
            out = pathlib.Path(d)
            result = {'status':'observed_native_route','captures':[]}
            finish_navigation(Live(), {}, result, out)
            saved = json.loads((out/'result.json').read_text())
            self.assertEqual(saved['status'], 'failed')
            self.assertEqual(saved['observed_status_before_cleanup'], 'observed_native_route')
            self.assertEqual(saved['after']['native']['player_life'], 7801)
            self.assertEqual(len(saved['cleanup_errors']), 2)

    def test_floor_identity_does_not_flatten_bridge_into_valley(self):
        nav=NativeMap([tile('a',[(0,0,0),(0,20,0)],[])])
        self.assertEqual(nav.nearest((0,20.8,0)),1)
        with self.assertRaises(BotFault): nav.nearest((0,10,0))
        with self.assertRaises(BotFault): nav.path(0,1)

    def test_exact_external_portal_joins_directed_paths(self):
        nav=NativeMap([tile('a',[(0,0,0),(1,0,0)],[(0,1)],boundaries=[1]),
                       tile('b',[(1,0,0),(2,0,0)],[(0,1)],boundaries=[0])])
        self.assertEqual(nav.path(0,3),[0,1,2,3])
        with self.assertRaises(BotFault): nav.path(3,0)

    def test_nearby_unconnected_nodes_cannot_invent_a_portal(self):
        for offset in (0., .1, 5.):
            nav=NativeMap([tile('a',[(0,0,0),(1,0,0)],[(0,1)]),
                           tile('b',[(1,offset,0),(2,offset,0)],[(0,1)])])
            with self.assertRaises(BotFault): nav.path(0,3)

    def test_blocked_native_edge_replans_without_repeating_it(self):
        nav=NativeMap([tile('a',[(0,0,0),(1,0,0),(1,0,2),(2,0,0)],
                            [(0,1),(1,3),(0,2),(2,3)])])
        self.assertEqual(nav.path(0,3),[0,1,3])
        self.assertEqual(nav.path(0,3,blocked=[(1,3)]),[0,2,3])
        with self.assertRaises(BotFault): nav.path(0,3,blocked=[(1,3),(2,3)])

    def test_hospital_event_uses_reachable_goal_instead_of_closest_island(self):
        from gameplay_bot.hospital_navigation import HospitalEventRoute
        route=HospitalEventRoute.__new__(HospitalEventRoute)
        route.nav=NativeMap([tile('ward',[(0,0,0),(4,0,0),(3.5,0,1)],[(0,2)])])
        route.root_offset=.425
        route.goal_candidates=[(0.,1),(1.12,2)]
        route.memory=Mock()
        route.memory.active.return_value=set()
        route.plan([0,.425,0])
        self.assertEqual(route.goal_node,2)
        self.assertEqual(route.route,[0,2])
        route.memory.active.return_value={(0,2)}
        with self.assertRaises(NoConnectedNativeRoute):route.plan([0,.425,0])

    def test_missing_split_group_is_retained_but_never_walked(self):
        nav=NativeMap([tile('a',[(0,0,0),(1,0,0)],[],cross=[{'from':0,'group':5,'to':1}])])
        self.assertEqual(len(nav.unresolved_portals),1)
        with self.assertRaises(BotFault): nav.path(0,1)

    def test_enemy_risk_can_choose_a_longer_route(self):
        nav=NativeMap([tile('a',[(0,0,0),(50,0,0),(50,0,60),(100,0,0)],
                            [(0,1),(1,3),(0,2),(2,3)])])
        self.assertEqual(nav.path(0,3),[0,1,3])
        self.assertEqual(nav.path(0,3,threats=[(50,0,0)]),[0,2,3])

    def test_danger_policy_uses_damage_alert_distance_and_elevation(self):
        n={'player_x':0,'player_y':0,'player_z':0,'player_life':1000,'not_alert':True}
        self.assertEqual(movement_mode(n,[]),'run')
        self.assertEqual(movement_mode(n,[(40,0,0)]),'crouch')
        self.assertEqual(movement_mode(n,[(15,0,0)]),'crawl')
        self.assertEqual(movement_mode(n,[(0,25,0)]),'run')
        self.assertEqual(movement_mode({**n,'not_alert':False},[]),'evade')
        self.assertEqual(movement_mode(n,[],1200),'retreat')
        self.assertEqual(movement_mode(n,[(68,0,0)],previous_mode='crouch'),'crouch')
        self.assertEqual(movement_mode(n,[(76,0,0)],previous_mode='crouch'),'run')
        self.assertEqual(movement_mode(n,[(35,0,0)],previous_mode='crawl'),'crawl')

    def test_posture_requires_native_ack_and_releases_on_each_change(self):
        class Live:
            def __init__(self): self.current='STAND';self.actions=[];self.released=False
            def release(self): self.released=True
            def observe(self,**_):return {'native':{'status_'+self.current:True}}
            def execute(self,step):
                self.actions.append(step);self.current='CRAWL' if step['seconds']>.5 else 'STAND'
        live=Live();set_posture(live,'crawl',sleep=lambda _:None)
        self.assertTrue(live.released);self.assertEqual(live.current,'CRAWL')
        self.assertEqual(live.actions[0]['name'],'gameplay.stance')

    def test_parser_bounds_and_unsupported_headers(self):
        with tempfile.TemporaryDirectory() as d:
            p=pathlib.Path(d)/'bad.nav2'
            for value in (b'',bytes(96),struct.pack('<4I',201403242,96,96,1)+bytes(80)):
                p.write_bytes(value)
                with self.assertRaises(BotFault):read_nav2(p)

    def test_obstacle_memory_respects_map_mission_and_expiry(self):
        with tempfile.TemporaryDirectory() as d:
            p=pathlib.Path(d)/'obstacles.json'
            memory=ObstacleMemory(p,'map1',(10040,10),100.)
            memory.remember(3,4,100.)
            self.assertEqual(ObstacleMemory(p,'map1',(10040,10),110.).active(110.),{(3,4),(4,3)})
            self.assertFalse(ObstacleMemory(p,'map2',(10040,10),110.).active(110.))
            self.assertFalse(ObstacleMemory(p,'map1',(10020,10),110.).active(110.))
            self.assertFalse(ObstacleMemory(p,'map1',(10040,10),401.).active(401.))


if __name__=='__main__':unittest.main()
