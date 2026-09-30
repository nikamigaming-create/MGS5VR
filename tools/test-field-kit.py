"""Run the separate MGS5VR developer test field kit (no LLM required)."""
import argparse
import pathlib
import webbrowser

from gameplay_bot.core import atomic_json
from gameplay_bot.fieldkit import FieldKit, make_server
from gameplay_bot.live import ROOT


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game-dir", type=pathlib.Path, required=True)
    parser.add_argument("--proxy", type=pathlib.Path, required=True)
    parser.add_argument("--controls-tool", type=pathlib.Path, default=ROOT / "build/Release/mgs5vr_controls.exe")
    parser.add_argument("--output", type=pathlib.Path, default=ROOT / "artifacts/test-field-kit")
    parser.add_argument("--port", type=int, default=8771)
    parser.add_argument("--open", action="store_true")
    args = parser.parse_args()
    kit = FieldKit(args.game_dir, args.proxy, args.controls_tool, args.output)
    server, connection = make_server(kit, args.port)
    atomic_json(args.output / "connection.json", connection)
    print("MGS5VR Test Field Kit: " + connection["url"], flush=True)
    print("Local access details: " + str((args.output / "connection.json").resolve()), flush=True)
    if args.open:
        webbrowser.open(connection["human_url"])
    try:
        server.serve_forever(poll_interval=.25)
    except KeyboardInterrupt:
        pass
    finally:
        kit.close()
        server.server_close()


if __name__ == "__main__":
    main()
