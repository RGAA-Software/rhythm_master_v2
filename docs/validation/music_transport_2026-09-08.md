# Music presentation time and synchronized controls, 2026-09-08

Studio and Windows Player now follow FilePlayback's estimated consumed-audio
position when local music is selected. Features and time come from one immutable
worker snapshot. Without music they use the existing monotonic local clock.
The portable PlaybackClock does not extrapolate a stalled media clock.

Timeline pause, seek and restart send the same intent to music playback. Music
panel controls and whole-track loops feed back through the same source clock.
Graph time, keyframe curves, video-node requests and temporal reset generations
follow it. A seek/loop clears particle, feedback and trail history and starts
simulation at the target time; it does not reconstruct the preceding history.
Timeline controls explain this behavior. Media duration is read-only there;
whole-track repeat is controlled in the audio panel.

Paused seeks still reevaluate the graph. The final consumed position at a pause
boundary is reflected even when the worker acknowledges it after the preceding
render frame. Identical media positions do not repeatedly advance temporal
feedback. Stop/EOF hold the media position; restart after EOF reloads playback.
Late-join package validation remains distinct from interactive history-reset
seeking. No networking behavior or dependency was added.

## Reuse

Studied FFmpeg n6.1.1 `fftools/ffplay.c`, LGPL-2.1-or-later, copyright Fabrice
Bellard and FFmpeg contributors, from the exact retained vcpkg source tree.
Source: https://github.com/FFmpeg/FFmpeg/blob/n6.1.1/fftools/ffplay.c.
Reviewed master-clock selection, clock serials, pause/seek and device-buffer
latency compensation. No ffplay code was copied or compiled into this project.
Its SDL player queues, codec types and mutable VideoState are incompatible with
the existing immutable graph inputs and host-owned render state. The existing
Rhythm FilePlayback/FrameClock and FFmpeg decoder adapters are reused; only the
typed time handoff and controls were added. No new dependency or license selected.

## Evidence and limits

- `out/transport-audio-tests.log`: real device output, canonical analysis,
  bounded queues, pause/seek/EOF/repeat/cancellation and recovery pass.
- `out/transport-audio-integration.log`: 149 actual audio/graph snapshots;
  consumed sample time equals graph core.time throughout pause, paused seek,
  rapid latest-seek replacement, resume, EOF and restart.
- `out/transport-studio-tests.log` and subsequent timeline interaction run:
  local loop/undo and real ImGui media pause/restart command handling pass.
- `out/transport-player-tests.log`: Player, image and video playback pass.
  Player contracts additionally cover media-master time, paused seek, loop
  and explicit stateful-history reset.
- `out/transport-final-tests.log`: real D3D music rendering, budget recovery
  and complete deploy smoke pass. Python redeployed both Windows apps with
  20 DLLs plus all resources.
- Both deployed applications pass startup with music already requested:
  `out/transport-player-audio-smoke.log`, `out/transport-studio-audio-smoke.log`.
  This exposed and fixed a transition from recycled to retained node targets
  while music was still paused/loading. The runtime tracks retirement explicitly,
  so newly observed outputs are rebuilt even when their inputs have not changed.
- `out/transport-android-tests.log`: PlaybackClock, Player and actual GLES
  contracts pass on e2b3b128, including device replacement and feedback.

This is an estimated device-consumption clock, not a hardware presentation
timestamp or a measured lip-sync bound. Whole-file looping drains and reloads;
gapless looping, arrangement tracks, audio carried by video nodes and encoded
audio/video export remain outstanding media work. Android app integration and
lifecycle acceptance are tracked separately from these native contracts.
