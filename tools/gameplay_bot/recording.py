"""Rotate native-eye/audio takes without pausing the gameplay executor.

This source precedes runtime crop/composition. Final compositor evidence remains
separate. Rotation gaps and capture failures are retained, never interpolated.
"""
import json
import pathlib
import shutil
import subprocess
import threading
import time

from .core import BotFault, atomic_json
from .live import ROOT, digest


def after_verified_arrival(enter_game, start_recording):
    """Start native recording only after the game has a source projection."""
    def enter_and_start():
        arrival = enter_game()
        if not isinstance(arrival, dict) or arrival.get("status") != "observed_arrival":
            raise BotFault("Session did not observe arrival; recording and cases were not started")
        start_recording()
        return arrival
    return enter_and_start


def validate_raw_video(video_stats, encoded_frames, encoded_duration, frame_rate=30.):
    """Validate CFR container cadence separately from accepted native samples.

    Media Foundation's CFR sink can repeat frames to fill cadence when source
    samples are dropped. `frames` in the native sidecar counts accepted source
    samples, whereas ffprobe's frame count includes the CFR output slots.
    """
    try:
        samples = int(video_stats["frames"])
        dropped = int(video_stats.get("dropped", 0))
        first = int(video_stats["first_qpc_100ns"])
        last = int(video_stats["last_qpc_100ns"])
        frames = int(encoded_frames)
        duration = float(encoded_duration)
        fps = float(frame_rate)
    except (KeyError, TypeError, ValueError, OverflowError) as error:
        raise BotFault("Native MP4/QPC metadata cannot validate the encoded cadence") from error
    if (samples < 1 or dropped < 0 or first < 0 or last < first or frames < 1
            or duration <= 0 or fps <= 0):
        raise BotFault("Native MP4/QPC metadata cannot validate the encoded cadence")
    qpc_span = (last - first) / 1e7
    frame_period = 1. / fps
    expected_slots = round(duration * fps)
    if abs(frames - expected_slots) > 1:
        raise BotFault("Raw MP4 frame count does not match its CFR duration")
    # The sidecar spans first-to-last source sample timestamps. A CFR stream
    # represents the last sample for one additional frame interval; allow one
    # cadence interval of timestamp/container rounding on either side.
    if duration < max(0., qpc_span - frame_period) or duration > qpc_span + 2 * frame_period:
        raise BotFault("Raw MP4 duration does not match the native QPC sample window")
    if frames < samples or frames - samples > dropped + 1:
        raise BotFault("Raw CFR cadence is inconsistent with accepted and dropped native samples")
    return {
        "native_accepted_sample_count": samples,
        "native_dropped_sample_count": dropped,
        "raw_video_encoded_frame_count": frames,
        "raw_video_frame_rate": fps,
        "raw_video_duration_seconds": duration,
        "native_qpc_span_seconds": qpc_span,
        "native_expected_cfr_frames": expected_slots,
        "cfr_repeat_or_fill_frame_count": frames - samples,
        "frame_count_semantics": "encoded CFR frames include cadence fill/repeats; native accepted_sample_count is separate",
    }


def mux_timeline(video_stats, audio_stats, video_duration_seconds=None):
    """Align measured PCM while retaining the complete native-eye stream."""
    first, last = video_stats.get("first_qpc_100ns"), video_stats.get("last_qpc_100ns")
    video_frames = video_stats.get("frames")
    audio_first, audio_frames = audio_stats.get("first_qpc_100ns"), audio_stats.get("frames")
    sample_rate = audio_stats.get("sample_rate")
    if (not isinstance(first, int) or not isinstance(last, int) or last < first
            or not isinstance(video_frames, int) or video_frames < 1
            or not isinstance(audio_first, int) or not isinstance(audio_frames, int) or audio_frames < 1
            or not isinstance(sample_rate, int) or sample_rate < 1):
        raise BotFault("Native audio/video metadata cannot define a measured mux timeline")
    qpc_duration = (last - first) / 1e7 + 1. / 30.
    video_duration = qpc_duration if video_duration_seconds is None else float(video_duration_seconds)
    if not 0 < video_duration <= qpc_duration + 1. / 30.:
        raise BotFault("Native MP4 duration is inconsistent with its sampled QPC interval")
    source_audio_duration = audio_frames / sample_rate
    offset = (audio_first - first) / 1e7
    aligned_audio_duration = max(0., source_audio_duration + offset)
    tail_padding = max(0., video_duration - aligned_audio_duration)
    filters = []
    if offset < 0:
        filters.extend((f"atrim=start={-offset:.7f}", "asetpts=PTS-STARTPTS"))
    elif offset > 0:
        filters.append(f"adelay={round(offset * sample_rate)}S:all=1")
    if tail_padding > 0:
        filters.append(f"apad=pad_dur={tail_padding:.7f}")
    return {
        "audio_filter": ",".join(filters) if filters else "anull",
        "video_duration_seconds": video_duration,
        "source_audio_duration_seconds": source_audio_duration,
        "audio_offset_seconds": offset,
        "audio_tail_padding_seconds": tail_padding,
        "audio_tail_padding_samples": round(tail_padding * sample_rate),
        "audio_tail_padding_reason": (
            "silence appended after measured source PCM to preserve every native video frame; "
            "padded interval is not captured game audio" if tail_padding else None
        ),
    }


class Recording:
    def __init__(self, game, events, pid, segment_seconds=90):
        self.events, self.pid = events, pid
        self.game = pathlib.Path(game).resolve()
        self.log_path = self.game / "mgs5vr.log"
        self.log_start_offset = None
        self.log_start_anchor = b""
        self.output = (events.output / "recording").resolve()
        self.output.mkdir()
        self.request = pathlib.Path(game) / "mgs5vr-recording.txt"
        self.segment_seconds = segment_seconds
        self.stop = threading.Event()
        self.segments = []
        self.error = None
        self.worker = None

    def start(self, timeout=5., clock=time.monotonic, sleep=time.sleep):
        if self.request.exists() and self.request.read_text(encoding="utf-8").strip():
            raise BotFault("Another native take is active")
        self.budget()
        # Only accept a first-frame marker appended after this run's recording
        # request. A matching line left by an older take is not readiness.
        try:
            self.log_start_offset = self.log_path.stat().st_size
            with self.log_path.open("rb") as log:
                log.seek(max(0, self.log_start_offset - 128))
                self.log_start_anchor = log.read(self.log_start_offset - max(0, self.log_start_offset - 128))
        except FileNotFoundError:
            self.log_start_offset = 0
            self.log_start_anchor = b""
        self.new_segment()
        self.worker = threading.Thread(target=self.watch, name="native-take-rotation", daemon=True)
        self.worker.start()
        self.wait_for_first_frame(timeout=timeout, clock=clock, sleep=sleep)

    def _new_first_frame_qpc(self, video):
        """Return only a complete, post-request native first-frame marker."""
        try:
            current = self.log_path.read_bytes()
        except FileNotFoundError:
            current = b""
        offset = self.log_start_offset or 0
        anchor_start = offset - len(self.log_start_anchor)
        unrotated = (len(current) >= offset and
                     (not self.log_start_anchor or current[anchor_start:offset] == self.log_start_anchor))
        if unrotated:
            chunks = (current[offset:],)
        else:
            # The native logger rotates its active log to .1 at 16 MiB. In
            # that case inspect the tail from our starting offset and the new
            # current file; never restart from the old prefix.
            previous = self.log_path.with_name(self.log_path.name + ".1")
            try:
                old = previous.read_bytes()
            except FileNotFoundError:
                old = b""
            chunks = (old[offset:] if len(old) >= offset else b"", current)
        prefix = f"Native video first frame encoded {video} qpc_100ns=".encode("utf-8")
        for chunk in chunks:
            for line in chunk.splitlines(keepends=True):
                if not line.endswith(b"\n"):
                    continue
                message = line.rstrip(b"\r\n").split(b" ", 1)
                if len(message) != 2 or not message[1].startswith(prefix):
                    continue
                raw_qpc = message[1][len(prefix):]
                if raw_qpc.isdigit():
                    qpc = int(raw_qpc)
                    if qpc > 0:
                        return qpc
        return None

    def wait_for_first_frame(self, timeout=5., clock=time.monotonic, sleep=time.sleep):
        if not self.segments:
            raise BotFault("Native first-frame wait has no owned segment")
        segment = self.segments[-1]
        video = segment["video"]
        deadline = clock() + timeout
        while True:
            self.check()
            owner = self.request.read_text(encoding="utf-8").strip() if self.request.exists() else ""
            if owner != str(video):
                raise BotFault("Recording request ownership changed before first source frame; no cases started")
            qpc = self._new_first_frame_qpc(video)
            if qpc is not None:
                self.events.emit("recording_segment_ready", path=str(video),
                                 first_frame_qpc_100ns=qpc, source="native_video_first_frame_encoded",
                                 wait_seconds=max(0., timeout - max(0., deadline - clock())))
                return qpc
            if clock() >= deadline:
                raise BotFault("Native first-frame readiness timed out; no cases started")
            sleep(min(.05, max(0., deadline - clock())))

    def budget(self):
        if shutil.disk_usage(self.output).free < 25*1024**3:
            raise BotFault("Recording reached the 25 GiB free-space reserve")
        if sum(p.stat().st_size for p in self.output.glob("*") if p.is_file()) >= 3*1024**3:
            raise BotFault("Recording reached its 3 GiB session budget")

    def new_segment(self):
        current_owner = self.request.read_text(encoding="utf-8").strip() if self.request.exists() else ""
        expected_owner = str(self.segments[-1]["video"]) if self.segments else ""
        if current_owner != expected_owner:
            raise BotFault("Native recording ownership changed; refusing to replace its request")
        number = len(self.segments)
        base = self.output / f"segment-{number:04d}"
        video, audio, stop = base.with_suffix(".mp4"), base.with_suffix(".wav"), base.with_suffix(".stop")
        if any(p.exists() for p in (video, audio, stop)):
            raise BotFault("Segment output already exists")
        with base.with_suffix(".audio.log").open("wb") as log:
            process = subprocess.Popen([str(ROOT / "build/Release/mgs5vr_audio_capture.exe"), str(self.pid), str(audio), str(stop)],
                                       stdout=log, stderr=log, creationflags=subprocess.CREATE_NO_WINDOW)
        current_owner = self.request.read_text(encoding="utf-8").strip() if self.request.exists() else ""
        if current_owner != expected_owner:
            stop.touch()
            try:
                process.wait(timeout=5)
            except Exception as error:
                raise BotFault("Recording ownership changed during rotation; candidate recorder did not stop") from error
            raise BotFault("Native recording ownership changed during rotation; request preserved")
        if self.segments:
            self.segments[-1]["stop"].touch()
        item = {"video": video, "audio": audio, "stop": stop, "process": process, "started": time.monotonic()}
        self.segments.append(item)
        # The recorder finishes the old path and switches to this fresh path
        # on its own worker. The behavior loop does not wait for the rotation.
        self.request.write_text(str(video), encoding="utf-8")
        self.events.emit("recording_segment_requested", path=str(video), source="native_source_eye_before_compositor")

    def watch(self):
        try:
            while not self.stop.wait(.25):
                self.budget()
                current = self.segments[-1]
                if current["process"].poll() is not None:
                    raise BotFault("Process-audio recorder exited unexpectedly")
                if self.request.read_text(encoding="utf-8").strip() != str(current["video"]):
                    raise BotFault("Native recording ownership changed")
                # Projection/resolution transitions can end an earlier take.
                if time.monotonic()-current["started"] >= self.segment_seconds or pathlib.Path(str(current["video"])+".json").exists():
                    self.new_segment()
        except Exception as error:
            self.error = str(error)
            self.events.emit("recording_error", error=self.error)
            self.stop.set()

    def check(self):
        """Fail at the next native observation after the recording worker faults."""
        if self.error:
            raise BotFault("Native recording failed during run: " + self.error)

    def close(self):
        self.stop.set()
        if self.worker:
            self.worker.join(timeout=3)
            if self.worker.is_alive():
                raise BotFault("Recording monitor did not stop; segment ownership remains unresolved")
        if self.segments:
            current_owner = self.request.read_text(encoding="utf-8").strip() if self.request.exists() else ""
            expected_owner = str(self.segments[-1]["video"])
            if current_owner == expected_owner:
                self.request.write_text("", encoding="utf-8")
            else:
                ownership_error = "Native recording ownership changed during close; request retained"
                self.error = self.error or ownership_error
                self.events.emit("recording_ownership_error", error=ownership_error,
                                 expected=expected_owner, observed=current_owner)
        for segment in self.segments:
            segment["stop"].touch()
        records = []
        for segment in self.segments:
            record = {"video": str(segment["video"]), "audio": str(segment["audio"]),
                      "source": "native_source_eye_before_runtime_crop_and_composition"}
            try:
                segment["process"].wait(timeout=10)
                deadline = time.monotonic() + 10
                video_meta = pathlib.Path(str(segment["video"])+".json")
                while not video_meta.exists():
                    if time.monotonic() >= deadline:
                        raise BotFault("Native video did not finalize; raw evidence retained")
                    time.sleep(.1)
                video = json.loads(video_meta.read_text())
                audio = json.loads(pathlib.Path(str(segment["audio"])+".json").read_text())
                if not video.get("complete") or not video.get("frames") or not audio.get("frames"):
                    raise BotFault("Incomplete native video/audio segment")
                raw_probe = subprocess.run(["ffprobe", "-v", "error", "-select_streams", "v:0",
                                            "-show_entries", "stream=duration,nb_frames", "-of", "json",
                                            str(segment["video"])], check=True, capture_output=True, text=True,
                                           timeout=30, creationflags=subprocess.CREATE_NO_WINDOW)
                raw_streams = json.loads(raw_probe.stdout).get("streams", [])
                if not raw_streams:
                    raise BotFault("Native take has no probeable source video stream")
                raw_frames = int(raw_streams[0].get("nb_frames", -1))
                raw_duration = float(raw_streams[0].get("duration", 0.))
                raw_fps = raw_streams[0].get("avg_frame_rate", "30/1")
                numerator, denominator = (float(part) for part in raw_fps.split("/", 1))
                encoded_rate = numerator / denominator if denominator else 0.
                raw_validation = validate_raw_video(video, raw_frames, raw_duration, encoded_rate)
                mux = mux_timeline(video, audio, raw_duration)
                combined = segment["video"].with_name(segment["video"].stem+"-av.mp4")
                subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-n", "-i", str(segment["video"]),
                                "-i", str(segment["audio"]), "-map", "0:v:0", "-map", "1:a:0", "-af", mux["audio_filter"],
                                "-c:v", "copy", "-c:a", "aac", "-b:a", "192k", "-movflags", "+faststart", str(combined)],
                               check=True, timeout=120, creationflags=subprocess.CREATE_NO_WINDOW)
                probe = subprocess.run(["ffprobe", "-v", "error", "-select_streams", "v:0",
                                        "-show_entries", "stream=duration,nb_frames", "-of", "json", str(combined)],
                                       check=True, capture_output=True, text=True, timeout=30,
                                       creationflags=subprocess.CREATE_NO_WINDOW)
                streams = json.loads(probe.stdout).get("streams", [])
                if not streams:
                    raise BotFault("Muxed native take has no video stream")
                muxed_frames = int(streams[0].get("nb_frames", -1))
                muxed_duration = float(streams[0].get("duration", 0.))
                if muxed_frames != raw_frames or muxed_duration + 1e-3 < raw_duration:
                    raise BotFault("Mux did not preserve every measured native video frame")
                record.update(status="captured", video_stats=video, audio_stats=audio,
                              output=str(combined), sha256=digest(combined), **mux,
                              **raw_validation,
                              raw_video_accepted_source_samples=video["frames"],
                              raw_video_native_dropped_samples=video.get("dropped", 0),
                              muxed_video_frame_count=muxed_frames,
                              muxed_video_duration_seconds=muxed_duration)
            except Exception as error:
                record.update(status="failed", error=str(error))
            records.append(record)
        result = {"segments": records, "error": self.error,
                  "continuous_gameplay": "capture_rotation_does_not_pause_executor",
                  "continuous_video": False, "visual_acceptance": "pending"}
        atomic_json(self.output / "index.json", result)
        return result

    def capture_tail(self, seconds=.4, clock=time.monotonic, sleep=time.sleep):
        """Keep neutral footage after the final case's outcome/capture bookkeeping."""
        if not 0 < seconds <= 2.:
            raise BotFault("Recording tail must be bounded to two seconds")
        self.events.emit("recording_tail", seconds=seconds, purpose="cover_final_case_outcome")
        deadline = clock() + seconds
        while clock() < deadline:
            self.check()
            sleep(min(.05, max(0., deadline - clock())))
        self.check()

    @staticmethod
    def failure_reason(result):
        if result.get("error"):
            return str(result["error"])
        for segment in result.get("segments", []):
            if segment.get("status") != "captured":
                return f"Recording segment failed: {segment.get('video', '<unknown>')}"
        return None
