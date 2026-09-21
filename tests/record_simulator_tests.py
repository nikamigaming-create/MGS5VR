"""Read-only regression tests for the final-eye capture tool's error boundary."""
import importlib.util
import pathlib
import unittest
import queue
from unittest import mock

spec = importlib.util.spec_from_file_location(
    "recorder", pathlib.Path(__file__).resolve().parents[1] / "tools/record-simulator.py")
recorder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(recorder)


class CaptureContracts(unittest.TestCase):
    def test_late_error_belongs_to_its_original_request(self):
        client = recorder.Operator.__new__(recorder.Operator)
        client.sequence = 4
        client.send = mock.Mock()
        client.messages = queue.Queue()
        client.messages.put({"id": 4, "error": {"message": "old input timed out"}})
        client.messages.put({"id": 5, "result": {"fresh": True}})
        self.assertEqual(client.request("tools/call", {}), {"fresh": True})

    def test_current_request_error_is_not_hidden(self):
        client = recorder.Operator.__new__(recorder.Operator)
        client.sequence = 0
        client.send = mock.Mock()
        client.messages = queue.Queue()
        client.messages.put({"id": 1, "error": {"message": "rejected input"}})
        with self.assertRaisesRegex(RuntimeError, "rejected input"):
            client.request("tools/call", {})

    def operator_result(self, result):
        client = recorder.Operator.__new__(recorder.Operator)
        client.request = mock.Mock(return_value=result)
        return client.call("openxr_set_controller_pose", {})

    def test_rpc_error_flag(self):
        with self.assertRaises(RuntimeError):
            self.operator_result({"isError": True})

    def test_text_error_without_flag(self):
        with self.assertRaisesRegex(RuntimeError, "unsupported"):
            self.operator_result({"content": [{"type": "text", "text": "Error: unsupported pose"}]})

    def test_valid_content(self):
        result = {"content": [{"type": "text", "text": "pose accepted"}]}
        self.assertEqual(self.operator_result(result), result)

    def test_failed_sequence_is_not_completed(self):
        sequence = mock.Mock()
        sequence.read_text.return_value = '[{"op":"pose","hand":"right","position":[0,0,0]}]'
        client = mock.Mock()
        client.call.side_effect = RuntimeError("rejected input")
        status = {}
        with mock.patch.object(recorder, "Operator", return_value=client):
            recorder.run_sequence(None, sequence, status)
        self.assertFalse(status["completed"])
        self.assertIn("rejected input", status["error"])
        self.assertEqual(len(status["actions"]), 1)
        client.close.assert_called_once()


if __name__ == "__main__":
    unittest.main()
