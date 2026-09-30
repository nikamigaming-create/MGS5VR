"""Evidence must describe the encoded video, not just native capture timestamps."""
import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('release_desk', ROOT / 'tools/field-guide/build_release_desk.py')
desk = importlib.util.module_from_spec(spec)
spec.loader.exec_module(desk)
audit_spec = importlib.util.spec_from_file_location('numeric_audit', ROOT / 'tools/field-guide/audit_numeric_tuning.py')
audit = importlib.util.module_from_spec(audit_spec)
audit_spec.loader.exec_module(audit)


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
