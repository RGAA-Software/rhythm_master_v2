# Editable time sections

`time.envelope` turns an explicit scalar time input into a 0–1 parameter gain.
The interval is half-open: start is included and end is excluded. Linear or
smooth fades use the existing curve evaluator; oversized fade lengths scale
proportionally to fit the interval. Zero fades produce a rectangular gate.
An explicit local time input remains independent of the global playback clock.

The Studio timeline lists root-scope envelopes as draggable bars. The author can
add one at the playhead, move it, trim either end and edit its fade properties.
Adding reuses a root clock or creates one and connects its time output in a
single undoable command. The envelope output is connected by the author to a
visual parameter such as opacity, blend amount or emission. Drag previews use
the existing snapshot/compile path and commit one undo step on release, including
when the row has scrolled outside the visible list. IDs are reserved by History.

No new dependency, serialization format or playback clock was introduced. The
existing registry, curve, graph edit commands, history and package compiler are
reused. Dear ImGui's existing InvisibleButton, DragScalar, draw list and list
clipper handle input, property editing and visible-row submission. See
[reuse record](../../provenance/time_sections.json).

## Verification

- Scalar contracts cover boundaries, smooth/linear/zero/overlapping fades and
  explicit local time caching. Existing graph and package contracts pass.
- `time_sections` verifies clock reuse, one-step add/undo/redo, stable ID
  allocation, invalid requests, connected parameter output, package round-trip
  and analytic seek eligibility.
- Actual ImGui tests in Chinese and English add at the playhead, move and trim
  both ends, undo once per gesture and release an offscreen dragged row. Mouse
  accuracy is checked within one pixel of timeline time, accounting for ImGui's
  integer pixel coordinates. Existing timeline interactions pass.
- Actual Studio loads the 197-instruction Resonance Live composition with an
  envelope replacing its opening curve, binds 128 seconds of original PCM,
  saves, publishes, clears and reopens it with the full waveform and section UI.
- USB Android `e2b3b128` passes the same portable scalar and command/package
  tests. The actual Studio package renders 240 music and 240 silent GLES frames
  at 640×360. Music p50/p95: 17.3802/19.3182 ms; silence: 17.1142/18.411 ms.
  Texture allocation remains 26,742,788 bytes. Mean RGB difference is 9.93483/255;
  music RMS reaches 0.22358 and silent RMS stays zero. This is not stable 60 fps.

Logs: `out/time-envelope-core-tests.log`, `out/time-section-command-tests.log`,
`out/time-section-final-tests.log`, `out/time-section-gpu-tests.log`,
`out/time-section-android-tests.log`, `out/time-section-gles-tests.log`.
Studio capture and published package are under
`out/windows-release/time-section-studio/477238474514400/`.
Windows deployment and the Android APK/relink material have been rebuilt.

## Remaining boundaries

This is graph parameter scheduling, not a complete media arrangement editor.
Root-scope bars do not yet provide component-local timeline editing, cue events,
multiple audio tracks, gapless playback or stateful history preroll. Zero gain
does not prune upstream dynamic GPU work. Old players reject the unknown
operator through existing package capability checks; they must be updated.

Android APK installation remains blocked by the device's USB installation
restriction. Native GLES/PCM tests do not establish application audio focus,
background/foreground lifecycle or application endurance acceptance. The
50 Basic + 50 Advanced visual-quality target is also unfinished.
