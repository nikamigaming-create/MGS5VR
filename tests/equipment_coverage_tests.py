"""Asset-free equipment inventory fixtures; never read a game installation."""
import importlib.util,pathlib,tempfile,unittest

root=pathlib.Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('equipment_coverage',root/'tools/equipment-coverage.py')
coverage=importlib.util.module_from_spec(spec);spec.loader.exec_module(coverage)

def gun(name,grade):
    return '{'+','.join(['TppEquip.WP_'+name]+['TppEquip.ST_None']*11+[str(grade)])+'}'

def development(name,kind='Sniper',identity=1):
    return f'TppMotherBaseManagement.RegCstDev{{p00={identity},p01=TppEquip.EQP_WP_{name},p02=TppMbDev.EQP_DEV_TYPE_{kind},p06="fixture_name"}}'

class ModernInventory(unittest.TestCase):
    def parse(self,table,dev,entries=None):
        with tempfile.TemporaryDirectory() as folder:
            folder=pathlib.Path(folder)
            source=folder/'parts.lua';source.write_text(table)
            definitions=folder/'dev.lua';definitions.write_text(dev)
            document={'source':'legacy-fixture.lua','source_sha256':'0'*64,'entries':entries or []}
            return coverage.tpp_chimera_inventory(document,source,definitions)

    def test_grades_remain_distinct_and_unrun(self):
        result=self.parse('local t={gunBasic={1,\n'+gun('fixture_a',1)+',\n'+gun('fixture_b',2)+'},other={}}',
                          development('fixture_a')+development('fixture_b',identity=2))
        self.assertEqual(result['modern_ids_added'],2)
        self.assertEqual([e['native_grade'] for e in result['entries']],[1,2])
        self.assertNotEqual(result['entries'][0]['id'],result['entries'][1]['id'])
        for entry in result['entries']:
            self.assertEqual(entry['classification'],'eligibility_to_check')
            self.assertEqual(entry['native_parts']['rear_sight'],'ST_None')
            self.assertIn('empty_reload',entry['tests'])
            self.assertTrue(all(t=={'result':'not_run','runs':[]} for t in entry['tests'].values()))

    def test_unknown_and_development_only_are_not_invented(self):
        result=self.parse('gunBasic={1,'+gun('fixture_a',1)+'}',development('fixture_b'))
        self.assertEqual(result['entries'][0]['native_type'],'Unknown')
        self.assertEqual(result['development_weapons_without_gunBasic'],['EQP_WP_fixture_b'])

    def test_duplicate_identity_rejected(self):
        with self.assertRaisesRegex(ValueError,'duplicate'):
            self.parse('gunBasic={1,'+gun('fixture_a',1)+','+gun('fixture_a',2)+'}',development('fixture_a'))

    def test_native_scientific_integer_notation(self):
        result=self.parse('gunBasic={1,'+gun('fixture_a',1)+'}',development('fixture_a',identity='1e3'))
        self.assertEqual(result['entries'][0]['development']['records'][0]['development_id'],1000)

    def test_multiple_development_records_preserve_one_runtime_id(self):
        result=self.parse('gunBasic={1,'+gun('fixture_a',1)+'}',development('fixture_a',identity=1)+development('fixture_a',identity=2))
        self.assertEqual(len(result['entries']),1)
        self.assertEqual(len(result['entries'][0]['development']['records']),2)

    def test_buddy_development_category_is_not_a_player_type_claim(self):
        result=self.parse('gunBasic={1,'+gun('fixture_a',1)+'}',development('fixture_a',identity=1)+development('fixture_a','Quiet',2))
        self.assertEqual(result['entries'][0]['native_type'],'Unknown')
        self.assertEqual(result['entries'][0]['development']['native_types'],['Sniper','Quiet'])

    def test_unrecognized_expression_not_executed(self):
        with self.assertRaises(ValueError):
            self.parse('gunBasic={1,'+gun('fixture_a',1).replace('TppEquip.ST_None','someFunction()',1)+'}',development('fixture_a'))

    def test_legacy_join_preserves_recorded_results(self):
        prior={'id':'EQP_WP_fixture_a','native_type':'Sniper','tests':{'actual_use':{'result':'observed'}}}
        result=self.parse('gunBasic={1,'+gun('fixture_a',1)+'}',development('fixture_a'),[prior])
        self.assertEqual(result['modern_ids_added'],0)
        self.assertEqual(result['entries'][0]['tests'],{'actual_use':{'result':'observed'}})

    def test_legacy_type_and_development_difference_are_both_kept(self):
        prior={'id':'EQP_WP_fixture_a','native_type':'Handgun','tests':{}}
        result=self.parse('gunBasic={1,'+gun('fixture_a',1)+'}',development('fixture_a','WalkerGear'),[prior])
        self.assertEqual(result['entries'][0]['native_type'],'Handgun')
        self.assertEqual(result['entries'][0]['development_type_difference'],['Handgun','WalkerGear'])

if __name__=='__main__':unittest.main()
