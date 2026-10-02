"""Navigation contracts: floors, explicit portals, obstacles and native stance."""
import math
import json
import pathlib
import struct
import sys
import tempfile
import unittest
import hashlib
from unittest.mock import patch

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]/'tools'))
from gameplay_bot.core import BotFault
from gameplay_bot.navigation import NativeMap, NavigationAtlas, ObstacleMemory, RouteProgress, arrived, movement_mode, read_nav2
from gameplay_bot.route import finish_navigation, navigate, set_posture


def tile(name, points, edges, *, boundaries=(), cross=(), group=4):
    return {'source':name, 'sha256':name*64, 'groups':[{'id':group,'nodes':points,
        'edges':[{'from':a,'to':b,'flags':1249} for a,b in edges],
        'boundaries':[{'node':i} for i in boundaries], 'cross_group':list(cross)}]}


class NavigationTests(unittest.TestCase):
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
