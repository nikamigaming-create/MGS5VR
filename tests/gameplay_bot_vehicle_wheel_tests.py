import copy
import math
import pathlib
import sys
import unittest
from contextlib import contextmanager
from unittest import mock

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / 'tools'))
from gameplay_bot.core import BotFault
from gameplay_bot.vehicle_wheel import sweep


class WheelSweepTests(unittest.TestCase):
    def fixture(self, fault=None):
        class Fake:
            def __init__(self):
                self.now=0.;self.held=[];self.releases=0;self.requests=[];self.angle=0.
                self.events=mock.Mock()
                self.bindings={'actions':[
                    {'name':name,'modifier':True,'contexts':['vehicle'],
                     'bindings':[{'gesture':'level','inputs':[token]}]}
                    for name,token in [('vehicle.wheel_grip','left_grip'),
                                       ('vehicle.right_wheel_grip','right_grip')]]}
            def clock(self):return self.now
            def sleep(self,n):self.now+=n
            def input(self,values,seconds,**options):
                self.held=values;self.requests.append((values,options))
            def release(self):self.held=[];self.releases+=1
            def call(self,name,args):
                if args['hand']=='left' and args['pose_type']=='grip':
                    q=args['orientation'];self.angle=-math.degrees(2*math.atan2(q[2],q[3]))
            def observe(self,**kwargs):
                self.now+=.02;stamp=1000+int(self.now*1000)
                axis=max(-1.,min(1.,self.angle/35)) if self.held else 0.
                physical=[0.]*11
                for value in self.held:physical[7 if value['hand']=='left' else 8]=1.
                native={'mission':30010,'location':10,'sequence':'Seq_Game_FreePlay',
                        'player_vehicle_id':25600,'status_ON_VEHICLE':True,
                        'player_vehicle_is_vehicle':True,'player_vehicle_is_helicopter':False,
                        'popup':False,'saving':False,'not_alert':True,
                        'player_x':1.,'player_y':2.,'player_z':3.}
                controls={'sample_ms':stamp,'age_ms':0,'context':'vehicle','rig_input':True,
                    'travel_mode':3,'native_packet_source':'xr_runtime_final_mapped_packet',
                    'native_axes':[int(axis*32767),0,0,0],'native_triggers':[0,0],
                    'native_buttons':0,'physical':physical,'sticks':[0.]*4}
                pose={'position':[0.,0.,0.],'orientation':[0.,0.,0.,1.]}
                rendered={'player_owner':1234,'activation':3,'sample_ms':stamp,
                    'presentation_focused':True,'driver_wheel_tracked':True,
                    'driver_wheel_axis':axis,'driver_wheel_hands':sum((1<<i) for i in range(2) if physical[7+i]),
                    'driver_left_wheel_grip':pose,'driver_right_wheel_grip':pose,'head_pose':pose,
                    'vehicle_weapon':{'requested':False,'ready':False}}
                for side in ('left','right'):
                    for kind in ('aim','grip'):rendered['raw_'+side+'_'+kind+'_tracked']=True
                delivery={'sample_ms':stamp,'caller_in_game':True,'packet_valid':True,
                    'source':'xr_mailbox','result_code':0,'axes':controls['native_axes'][:],
                    'buttons':0,'triggers':[0,0]}
                if self.held and fault=='pedal':controls['native_triggers'][1]=255
                if self.held and fault=='moving':native['player_z']+=.06
                if self.held and fault=='lost_owner':rendered['player_owner']=5678
                if axis and fault=='wrong_delivery':delivery['axes'][0]*=-1
                if axis and fault=='stale_delivery':delivery['sample_ms']-=151
                return {'scene':'gameplay','now_ms':stamp,'activation':3,'camera_active':True,
                    'camera_suspended':False,'menu':False,'idroid':False,'gamepad':False,
                    'controls':controls,'rendered':rendered,'native':native,
                    'xinput_delivery':{'index0':delivery}}
        live=Fake()
        native=live.observe()['native']
        step={'op':'action','name':'vehicle.wheel_grip','wheel_sweep_degrees':35,'native_before':native}
        return live,step

    @staticmethod
    @contextmanager
    def saved(*args):
        yield {'position':[0.,0.,0.],'orientation':[0.,0.,0.,1.]}

    def test_signed_left_and_both_sweeps_require_delivered_response_and_return_neutral(self):
        for both,degrees in ((False,35),(True,-35)):
            live,step=self.fixture();step.update(wheel_sweep_degrees=degrees)
            if both:step.update(name='vehicle.right_wheel_grip',include_left_wheel=True)
            with mock.patch('gameplay_bot.vehicle_wheel.preserved_controller_pose',self.saved):
                result=sweep(live,step,clock=live.clock,sleep=live.sleep)
            self.assertEqual(result['state']['rendered']['driver_wheel_hands'],3 if both else 1)
            self.assertGreater(result['state']['xinput_delivery']['index0']['axes'][0]*degrees,0)
            self.assertAlmostEqual(live.angle,0.)
            self.assertEqual(live.releases,1)
            self.assertLessEqual(live.requests[0][1]['lease_seconds']+.15,3.)

    def test_wrong_or_stale_delivery_never_counts_as_steering_and_releases(self):
        for fault in ('wrong_delivery','stale_delivery','pedal','moving','lost_owner'):
            with self.subTest(fault=fault):
                live,step=self.fixture(fault)
                with mock.patch('gameplay_bot.vehicle_wheel.preserved_controller_pose',self.saved),self.assertRaises(BotFault):
                    sweep(live,step,clock=live.clock,sleep=live.sleep)
                self.assertEqual(live.releases,1)
                self.assertFalse(live.held)


if __name__=='__main__':unittest.main()
