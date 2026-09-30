"""External supervisor client. Scripts and LLMs use the same local observation/action API."""
import argparse
import json
import pathlib
import time
import urllib.error
import urllib.request


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--connection", type=pathlib.Path, required=True)
    parser.add_argument("--role", choices=("human", "llm"), default="llm", help="LLM by default; human mode also supports local scripts")
    parser.add_argument("command", choices=("state", "observe", "connect", "continue", "run", "stop", "disconnect"))
    parser.add_argument("--scenarios", nargs="+", help="Catalog IDs, e.g. equipment commands binoculars")
    parser.add_argument("--observation-id", help="Required for input; use the ID whose images you reviewed")
    parser.add_argument("--image-sha256", help="Required for input; SHA256 of a reviewed capture from that observation")
    parser.add_argument("--wait", action="store_true")
    args = parser.parse_args()
    connection = json.loads(args.connection.read_text(encoding="utf-8"))
    def request(endpoint, body=None):
        data = None if body is None else json.dumps(body).encode()
        req = urllib.request.Request(connection["url"] + endpoint, data=data,
            headers={"Authorization": "Bearer " + connection["tokens"][args.role], "Content-Type": "application/json"})
        with urllib.request.urlopen(req, timeout=10) as response:
            return json.load(response)
    state = request("/api/state")
    previous_runs = len(state["results"])
    if args.command != "state":
        if args.command in ("run", "continue") and (not args.observation_id or not args.image_sha256):
            parser.error("Review the observation's captures and provide --observation-id and --image-sha256")
        request("/api/command", {"action": args.command, "epoch": state["epoch"], "scenarios": args.scenarios,
            "observation_id": args.observation_id, "reviewed_capture_sha256": args.image_sha256})
        if args.wait:
            deadline = time.monotonic() + 600
            while True:
                state = request("/api/state")
                if not state["busy"]:
                    break
                if time.monotonic() >= deadline:
                    raise RuntimeError("Wait expired; use state or stop to inspect the still-running task")
                time.sleep(.5)
        else:
            state = request("/api/state")
    print(json.dumps(state, indent=2))
    if args.wait and args.command == "run":
        added = state["results"][previous_runs:]
        if len(added) != len(args.scenarios or []) or any(item["status"] != "observed_pass" for item in added):
            raise SystemExit(1)
    if args.wait and args.command in ("connect", "observe", "continue") and (not state["connected"] or state["status"] != "Ready"):
        raise SystemExit(1)


if __name__ == "__main__":
    try:
        main()
    except urllib.error.HTTPError as error:
        raise SystemExit(error.read().decode())
