# Live native actions

The development build exposes a low-latency named-pipe bridge when
`[diagnostics] native_actions = 1` is enabled. The pipe reader runs on a
background thread; Lua evaluation still runs on the game's native transaction
thread. This keeps native state access safe without the old 250 ms filesystem
poll on every transaction.

## Send one action

From the repository root:

```powershell
python .\tools\native-actions.py --script "return TppMission.GetMissionID()"
```

For a longer script, use a UTF-8 file:

```powershell
python .\tools\native-actions.py --file .\artifacts\my-action.lua
```

Native button pulses do not wait for Lua readiness, so they can cross the
title/loading handoff:

```powershell
python .\tools\native-actions.py --input a
```

Supported inputs are `a`, `b`, `x`, `y`, `start`, `back`, and `release`.
`a` is the useful Continue/"press any key" pulse for the native loading tip.

The client prints one JSON response:

```json
{"id":1,"status":0,"queued_ms":2,"execution_us":27,"result":"10036"}
```

`queued_ms` measures time waiting for the native Lua transaction. `execution_us`
measures the Lua/native action itself. A command waits until `TppMain` and
`GameObject` are ready; it is not discarded during title/loading transitions.

## Cutscene input guard

Presentation detection checks both `DemoDaemon.IsDemoPlaying()` and the active
mission's `s10010_sequence.IsDemoPlaying()` flag. The `Seq_Demo_*` name remains
a fallback for scenes without either API. While an actual demo is active, VR
keeps the authored stereo camera and body visible but sends no left- or
right-stick movement/look axes to the native game. Native buttons remain
available for the game's own menu/skip prompts.

Run `python .\tools\test-native-presentation.py` to check these states in the
game's Lua 5.1 VM. Its fixtures replace the queried globals in an isolated
environment and do not alter mission, actor, or save state.

## Send a batch

Create a JSON array containing Lua strings or objects with a `script` property:

```json
[
  {"script":"return TppMission.GetMissionID()"},
  {"script":"return tostring(vars.buddyType)"},
  {"script":"return tostring(TppBuddyService.CanSortieBuddyType(BuddyType.DOG))"}
]
```

Execute the commands in order, waiting for each response before the next:

```powershell
python .\tools\native-actions.py --batch .\artifacts\actions.json
```

The pipe thread owns all pipe I/O; the native Lua thread only queues replies.
This prevents a waiting pipe read from blocking the game's update thread.
The old `mgs5vr-native-actions.lua` request file remains supported
for compatibility, but it is slower and is intended for legacy probes only.

## Native-eye capture

Set an absolute output path in the installed game's
`mgs5vr-recording.txt`:

```text
C:/Captures/MGS5VR/live-native-eye.mp4
```

The recorder captures the submitted native eye texture, not a desktop mirror.
It writes a matching `.json` file when the take finalizes with frame count,
resolution, dropped copies, projection metadata, and completion status. Keep
capture output on a volume with at least 25 GiB free.

The action response timings and the recorder metadata can be joined by the
operator's wall-clock log when a synchronized per-event timeline is needed.
The pipe does not fabricate frames or alter native game results.
