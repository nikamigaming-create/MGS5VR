"""Read the current game's existing native Press Start show-hook publication."""
import pathlib
import re
import time


class StartupEvidence:
    def __init__(self, log_path, process_creation, clock_ms=None):
        match = re.fullmatch(r"/Date\((\d+)(?:[+-]\d{4})?\)/", process_creation or "")
        if not match:
            raise ValueError("Cannot bind native startup evidence to the game process creation time")
        self.started_ms = int(match.group(1))
        self.path = pathlib.Path(log_path)
        self.offset = 0
        self.partial = b""
        self.prompt_ms = None
        self.clock_ms = clock_ms or (lambda: time.time_ns() // 1_000_000)

    def read(self):
        try:
            with self.path.open("rb") as stream:
                size = stream.seek(0, 2)
                if size < self.offset:
                    self.offset, self.partial, self.prompt_ms = 0, b"", None
                stream.seek(self.offset)
                while self.offset < size:
                    chunk = stream.read(min(1024 * 1024, size - self.offset))
                    if not chunk:
                        break
                    self.offset += len(chunk)
                    lines = (self.partial + chunk).split(b"\n")
                    self.partial = lines.pop()
                    for line in lines:
                        match = re.fullmatch(rb"(\d+) Native Press Start prompt opened\r?", line)
                        if match and int(match.group(1)) >= self.started_ms:
                            self.prompt_ms = int(match.group(1))
        except OSError as error:
            return {"press_start_ready": False, "startup_evidence_error": str(error)}
        age = self.clock_ms() - self.prompt_ms if self.prompt_ms is not None else None
        # The native Show hook starts the title fade; it does not mean the
        # prompt already accepts START. Retain that distinction in evidence.
        return {"press_start_ready": age is not None and age >= 2000,
                "press_start_opened_unix_ms": self.prompt_ms,
                "press_start_age_ms": age,
                "press_start_settle_ms": 2000,
                "startup_evidence_source": "current_process_native_show_hook"}
