# Continuous queued music loops

Normal whole-track repeat now keeps the existing SDL output stream open and
appends the next FFmpeg-decoded iteration before the preceding tail drains.
It no longer inserts an output flush, close/reopen or loading state at each
loop. The decoder is reopened through its existing exact Seek(0) path; there is
still one media backend and no whole-song PCM cache or second player.

This follows the existing SDL queue API rather than replacing the device or
resampler. SDL explicitly warns that appending after a flush can introduce gaps:
[SDL_FlushAudioStream](https://wiki.libsdl.org/SDL3/SDL_FlushAudioStream).
Its input queue reports queued source-format bytes, not hardware presentation:
[SDL_GetAudioStreamQueued](https://wiki.libsdl.org/SDL3/SDL_GetAudioStreamQueued).
The project keeps its fixed stereo 48 kHz input contract and existing consumption
estimate. It does not claim measured hardware gaplessness or latency.

## Time, ownership and bounds

Loop decoding reserves a monotonically increasing future generation. The
analysis queue retains each block's local sample coordinates and generation,
while submitted/consumed device-frame counters remain continuous. Analysis and
the public playback generation change only when consumption reaches the first
sample of the next iteration, not when that iteration is decoded ahead.
The Analyzer resets at that boundary, so features from the prior loop cannot
leak into the current loop's time. A new FFT window is required before fresh
features become valid, as with the existing seek contract.

Source-request acknowledgment is separate from audible loop generations.
Load/seek/stop cancel old work and reset the device; reserved loop generations
cannot acknowledge source replacement or cause retired imports to be released
early. `PlaybackSnapshot` now exposes continuous submitted/consumed counters
for diagnostics. These reset on load/seek, not during ordinary looping.

Output retains the existing 24,000-frame queue cap. Analysis retains at most
32,768 frames **and** 65,536 allocated float slots (256 KiB PCM, plus bounded
block metadata); short blocks cannot hide excessive vector capacity. Queue
validation rejects invalid generations, gaps and excess capacity before mutation.
Pausing freezes consumption; replacement/cancellation keep their existing
worker/RAII cleanup. Turning looping off finishes the iteration already
submitted to the bounded queue. Enabling loop after playback has already ended
uses the existing explicit restart path.

## Verification

- `out/loop-analysis-tests.log`: exact boundaries, nonzero initial seek origin,
  decode-ahead isolation, generation reset at consumption, local FFT timestamps,
  continuous device coordinates, rejected append recovery and allocated-capacity
  limits pass. FFT assertions respect its existing 4,096-sample window and
  800-sample analysis cadence.
- `out/continuous-loop-counters-tests.log`: real Windows output plays two complete
  one-second loops without returning to loading. Submitted/consumed counters
  exceed 96,000 frames continuously while source acknowledgment stays fixed.
  Pause/seek/stop, embedded bytes, file ranges, cancellation and EOF pass.
- `out/continuous-loop-final-tests.log`: repeat, analysis, source boundaries,
  actual music import ownership/suspend/focus behavior and shared music transport
  are automated with the correct 16-second music fixture. The import/transport
  checks are now registered in CTest for reproducible reruns.
- `out/continuous-loop-player-tests.log`: small/large packaged soundtracks and
  actual Studio music smoke pass. Windows Studio and Player are rebuilt and
  deployed with all 20 DLLs.
- `out/continuous-loop-android-tests.log`: USB Android `e2b3b128` passes the same
  native analysis/loop-boundary/capacity contracts.
- `out/continuous-loop-android-build.log`: shared Android playback and the APK
  with matching relink materials rebuild successfully. Device policy still
  blocks installation; Java/SDL device output and lifecycle remain unaccepted.

No crossfade conceals a discontinuous source waveform. OS scheduling, slow
decoder reopen, very short clips and device underruns can still cause audible
issues; no universal hardware gaplessness guarantee or endurance result is
inferred from these bounded tests. Full multitrack scheduling and arbitrary
loop-region editing remain separate roadmap items.
