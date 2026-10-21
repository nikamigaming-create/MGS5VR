import copy
import sys
import unittest
from pathlib import Path

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from gameplay_bot.weapon_probe import check_recovery_attack, require_supported_shots
from gameplay_bot.arsenal import inventory
from gameplay_bot.core import BotFault


class RecoveryEvidenceTests(unittest.TestCase):
    def setUp(self):
        native={}
        for slot in ('PRIMARY_HIP','PRIMARY_BACK','SECONDARY'):
            native['weapon_'+slot]=871
            native['ammo_in_weapon_'+slot]=30
        self.state={'native':native,'controls':{
            'physical':[0]*10+[1], 'native_triggers':[255,0]}}
        self.ammo=inventory(native)

    def test_sampled_suppression_and_release(self):
        check_recovery_attack(self.state,self.ammo,held=True)
        self.state['controls']['physical'][10]=0
        check_recovery_attack(self.state,self.ammo,held=False)

    def test_attack_leak_fails_even_without_ammunition_delta(self):
        self.state['controls']['native_triggers'][1]=255
        with self.assertRaises(BotFault):check_recovery_attack(self.state,self.ammo,held=True)

    def test_discharge_fails_even_when_latest_mapped_trigger_is_zero(self):
        self.state['native']['ammo_in_weapon_PRIMARY_HIP']-=1
        with self.assertRaises(BotFault):check_recovery_attack(self.state,self.ammo,held=True)

    def test_missing_or_unsampled_input_cannot_pass(self):
        for physical in ([],[0]*11):
            state=copy.deepcopy(self.state);state['controls']['physical']=physical
            with self.assertRaises(BotFault):check_recovery_attack(state,self.ammo,held=True)


class SupportedShotEvidenceTests(unittest.TestCase):
    def test_complete_observed_shots_pass(self):
        require_supported_shots([{'discharge':{'status':'pass'},
            'support_retained':True,'trigger_sampled':True} for _ in range(3)])

    def test_partial_or_failed_shots_cannot_be_reported_as_pass(self):
        rows=[{'discharge':{'status':'pass'},'support_retained':True,
               'trigger_sampled':True} for _ in range(3)]
        with self.assertRaises(BotFault):require_supported_shots(rows[:1])
        for field,value in (('discharge',{'status':'unknown'}),
                            ('support_retained',False),('trigger_sampled',False)):
            failed=copy.deepcopy(rows);failed[1][field]=value
            with self.assertRaises(BotFault):require_supported_shots(failed)


if __name__=='__main__':unittest.main()
