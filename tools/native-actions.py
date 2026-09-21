"""Send native Lua actions through the low-latency MGS5VR named pipe."""
import argparse
import ctypes
from ctypes import wintypes
import json
import msvcrt
import pathlib
import struct
import sys
import time

PIPE = r"\\.\pipe\MGS5VR.NativeActions"
REQUEST = struct.Struct("<QI")
RESPONSE = struct.Struct("<QiIQQ")
MAX_SCRIPT = 1024 * 1024
KERNEL32 = ctypes.WinDLL("kernel32", use_last_error=True)
KERNEL32.PeekNamedPipe.argtypes = [wintypes.HANDLE, wintypes.LPVOID, wintypes.DWORD,
                                 wintypes.LPDWORD, wintypes.LPDWORD, wintypes.LPDWORD]
KERNEL32.PeekNamedPipe.restype = wintypes.BOOL


def read_exact(stream, size, deadline):
    chunks = bytearray()
    while len(chunks) < size:
        available = wintypes.DWORD()
        handle = msvcrt.get_osfhandle(stream.fileno())
        if not KERNEL32.PeekNamedPipe(handle, None, 0, None, ctypes.byref(available), None):
            raise ctypes.WinError(ctypes.get_last_error())
        if not available.value:
            if time.monotonic() >= deadline:
                raise RuntimeError("native action response timed out; action completion is unknown")
            time.sleep(0.01)
            continue
        chunk = stream.read(min(size - len(chunks), available.value))
        if not chunk:
            raise RuntimeError("native action pipe closed before the response arrived")
        chunks.extend(chunk)
    return bytes(chunks)


def connect(timeout):
    deadline = time.monotonic() + timeout
    while True:
        try:
            return open(PIPE, "r+b", buffering=0)
        except OSError as error:
            if time.monotonic() >= deadline:
                raise RuntimeError(f"cannot connect to {PIPE}: {error}") from error
            time.sleep(0.02)


def load_scripts(args):
    if args.input is not None:
        return [f"input:{args.input}"]
    if args.script is not None:
        return [args.script]
    if args.file is not None:
        return [pathlib.Path(args.file).read_text(encoding="utf-8")]
    values = json.loads(pathlib.Path(args.batch).read_text(encoding="utf-8"))
    if not isinstance(values, list) or not values:
        raise ValueError("--batch must contain a non-empty JSON array")
    scripts = []
    for value in values:
        if isinstance(value, str):
            scripts.append(value)
        elif isinstance(value, dict) and isinstance(value.get("script"), str):
            scripts.append(value["script"])
        else:
            raise ValueError("each batch item must be a Lua string or {\"script\": string}")
    return scripts


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--script", help="Lua source to execute")
    source.add_argument("--file", help="UTF-8 Lua file to execute")
    source.add_argument("--batch", help="JSON array of Lua strings or script objects")
    source.add_argument("--input", choices=("a", "b", "x", "y", "start", "back", "release"),
                        help="Publish one native button pulse without waiting for Lua readiness")
    parser.add_argument("--timeout", type=float, default=20.0)
    parser.add_argument("--raw", action="store_true", help="print only Lua results")
    args = parser.parse_args()
    if args.timeout <= 0:
        raise ValueError("--timeout must be positive")
    scripts = load_scripts(args)
    if any(not script or len(script.encode("utf-8")) > MAX_SCRIPT for script in scripts):
        raise ValueError("scripts must be non-empty and at most 1 MiB UTF-8")
    with connect(args.timeout) as pipe:
        for index, script in enumerate(scripts):
            request_id = time.time_ns() ^ index
            payload = script.encode("utf-8")
            pipe.write(REQUEST.pack(request_id, len(payload)))
            pipe.write(payload)
            deadline = time.monotonic() + args.timeout
            response_id, status, length, queued_ms, execution_us = RESPONSE.unpack(
                read_exact(pipe, RESPONSE.size, deadline))
            if length > MAX_SCRIPT:
                raise RuntimeError("native action response exceeds 1 MiB")
            result = read_exact(pipe, length, deadline).decode("utf-8", errors="replace")
            if response_id != request_id:
                raise RuntimeError(f"response id {response_id} did not match request {request_id}")
            record = {
                "id": response_id,
                "status": status,
                "queued_ms": queued_ms,
                "execution_us": execution_us,
                "result": result,
            }
            if args.raw:
                print(result)
            else:
                print(json.dumps(record, ensure_ascii=True))
            if status != 0:
                return 2
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, ValueError) as error:
        print(f"native-actions: {error}", file=sys.stderr)
        raise SystemExit(1)
