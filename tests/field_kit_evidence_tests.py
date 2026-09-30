"""Evidence must describe the encoded video, not just native capture timestamps."""
import importlib.util
import hashlib
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('release_desk', ROOT / 'tools/field-guide/build_release_desk.py')
desk = importlib.util.module_from_spec(spec)
spec.loader.exec_module(desk)
audit_spec = importlib.util.spec_from_file_location('numeric_audit', ROOT / 'tools/field-guide/audit_numeric_tuning.py')
audit = importlib.util.module_from_spec(audit_spec)
audit_spec.loader.exec_module(audit)
community_spec = importlib.util.spec_from_file_location('community_bundle', ROOT / 'tools/community_verification.py')
community = importlib.util.module_from_spec(community_spec)
community_spec.loader.exec_module(community)


class CleanSourceEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.source = self.root / 'ledger.json'

    def tearDown(self):
        self.temp.cleanup()

    def bundle(self, item):
        reports = [{'id': f'R{i:02}', 'claims': [{'id': f'C{i:02}',
                    'status': 'verified_automated', 'evidence': [item]}]} for i in range(1, 56)]
        self.source.write_text(json.dumps({'reports': reports}), encoding='utf-8')
        return community.bundle(self.root, self.source, self.root / 'output')

    def test_missing_private_evidence_preserves_historical_claim_without_proof_url(self):
        data = self.bundle({'path': 'artifacts/native-result.txt', 'sha256': 'a' * 64})
        self.assertEqual(len(data['reports']), 55)
        for report in data['reports']:
            claim = report['claims'][0]
            self.assertEqual(claim['status'], 'verified_automated')
            self.assertIs(claim['evidence'][0]['available'], False)
            self.assertNotIn('url', claim['evidence'][0])
        self.assertEqual(data['bundled_evidence'], {'files': 0, 'bytes': 0, 'unavailable_files': 1})

    def test_present_reviewed_evidence_is_bundled_once_with_valid_bytes(self):
        raw = b'actual native observation'
        (self.root / 'result.txt').write_bytes(raw)
        sha = hashlib.sha256(raw).hexdigest()
        data = self.bundle({'path': 'result.txt', 'sha256': sha})
        item = data['reports'][0]['claims'][0]['evidence'][0]
        self.assertIs(item['available'], True)
        self.assertEqual((self.root / 'output' / item['url']).read_bytes(), raw)
        self.assertEqual(data['bundled_evidence']['files'], 1)

    def test_changed_present_evidence_still_rejects_build(self):
        (self.root / 'result.txt').write_bytes(b'changed evidence')
        with self.assertRaisesRegex(ValueError, 'has changed'):
            self.bundle({'path': 'result.txt', 'sha256': 'a' * 64})

    def test_missing_file_does_not_bypass_path_or_identity_validation(self):
        for item in ({'path': '../outside.txt', 'sha256': 'a' * 64},
                     {'path': 'missing.txt', 'sha256': 'invalid'}):
            with self.subTest(item=item), self.assertRaisesRegex(ValueError, 'Invalid evidence'):
                self.bundle(item)

    def test_review_keeps_only_explicit_capture_reference_when_it_is_missing(self):
        raw = json.dumps({'captures': [{'path': 'capture.png', 'sha256': 'b' * 64}]}).encode()
        (self.root / 'visual-review.json').write_bytes(raw)
        data = self.bundle({'path': 'visual-review.json', 'sha256': hashlib.sha256(raw).hexdigest()})
        item = data['reports'][0]['claims'][0]['evidence'][0]
        self.assertIs(item['available'], True)
        self.assertIs(item['related'][0]['available'], False)
        self.assertNotIn('url', item['related'][0])
        self.assertEqual(data['bundled_evidence']['unavailable_files'], 1)


class EncodedCoverageTests(unittest.TestCase):
    def test_truncated_mux_rejects_action_even_with_complete_native_segment(self):
        with self.assertRaisesRegex(ValueError, 'whole action'):
            desk.clip_window(10000000, 310000000, 295000000, 305000000, 29.0)

    def test_first_frame_must_precede_action(self):
        with self.assertRaisesRegex(ValueError, 'whole action'):
            desk.clip_window(10000000, 310000000, 9999999, 20000000, 30.0)

    def test_only_optional_tail_is_shortened_to_encoded_video(self):
        self.assertEqual(desk.clip_window(10000000, 310000000, 290000000, 299000000, 29.0),
                         (288000000, 300000000))

    def test_video_padding_cannot_claim_native_frames_after_capture(self):
        self.assertEqual(desk.clip_window(10000000, 300000000, 290000000, 299000000, 35.0),
                         (288000000, 300000000))


class NumericInventoryTests(unittest.TestCase):
    def test_cpp_ignores_comments_raw_strings_and_identifiers(self):
        source = 'const float depth=.08f; // 123\n/* 987\n654 */\nauto s=R"tag(4.5f)tag";\nint r8=0x20;'
        values = list(audit.literals(source))
        self.assertEqual([(v['literal'], v['line']) for v in values], [('.08f', 1), ('0x20', 5)])

    def test_lua_long_comments_and_strings_do_not_count(self):
        source = '--[=[ 123\n456 ]=]\na = [==[ 789 ]==]\nb = 1e-3 -- 456\n'
        self.assertEqual([v['literal'] for v in audit.literals(source, lua=True)], ['1e-3'])

    def test_actual_masm_offsets_are_visible_without_register_digits(self):
        source = '; preserve xmm0 and 25 bytes\nsub rsp,0D8h\nmovdqu [rsp+60h],xmm0\ncmp dword ptr [rsi+8],4\n'
        self.assertEqual([v['literal'] for v in audit.literals(source, assembly=True)], ['0D8h', '60h', '8', '4'])


if __name__ == '__main__':
    unittest.main()
