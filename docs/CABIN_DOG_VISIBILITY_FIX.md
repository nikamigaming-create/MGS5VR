# Cabin D-Dog visibility, October 11

The native ACC D-Dog now stays visible before opening handheld iDroid and after
stowing it. On development DLL `3e6e3d6e9818`, five actual-game checkpoints
retained native model opacity 1.0: closed handset, head turn, open handset,
stowed handset and positional lean. Both compositor eyes were reviewed at all
five checkpoints. D-Dog is the naturally authored retail actor; these tests do
not spawn, move or replace him or write mission progress.

The same scene on the published `ba5c0ccbf406` build reproduced opacity 0.01
with iDroid closed and 1.0 while open. This establishes the camera/fade defect
on that baseline; it does not establish which historical change introduced it.

A bounded four-byte hardware write watchpoint identified the native opacity
writer at RVA `c5ea15` in the CNP character publisher `c5e860`. That publisher
passes the stock camera point to its native camera-nearness query `c8c300`.
Our rendered VR cabin camera uses a separate tracked pose. Opening iDroid takes
the native publisher's full-opacity branch, which explains why the dog appears
while the handset is open.

The correction in `src/player_visibility.cpp` supplies the current rendered VR
camera position to that existing query for the verified native cabin dog call.
It preserves the native bone distances, fade limits, smoothing and publication.
It checks the exact call site, publisher owner, fresh tracked cabin frame, typed
actor/component/model and dog resource before substituting the camera point.
Other callers and unavailable identities retain the original native calculation.
No visibility groups, actor lifecycle or forced opacity writes were added.
Earlier unsuccessful publisher hook experiments were removed.

Evidence is retained under `artifacts/dev/idroid-stick-dog-current/`:

- `dog-published-baseline/result.json`: the baseline's closed/open/closed fade.
- `dog-native-write-instruction/dog-before-idroid-writer.json`: the actual
  native store instruction and detached debugger receipt.
- `dog-tracked-camera-after/result.json`: five checked development checkpoints
  with native model identities/opacity and paired final-eye PNGs.
- `dog-tracked-camera-after/visual-review.json`: scoped paired-eye review.
- `dog-tracked-camera-build.log`: 55/55 automated groups and installer checks.

This is stationary native ACC simulator acceptance for the reported handset
visibility failure, including turn and lean. It does not certify every dog
interaction, helicopter flight or physical headset comfort. The published
`play/` slot remains unchanged; use `Play-Dev.cmd` for the candidate.
