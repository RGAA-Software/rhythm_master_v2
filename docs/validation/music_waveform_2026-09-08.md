# Music waveform overview in the Studio timeline

Studio's timeline now displays the selected local music as a complete peak
envelope with a seconds ruler and playback cursor. Clicking or dragging and
releasing inside the waveform emits one seek through the existing music clock.
Dragging does not repeatedly restart exact audio decoding; releasing outside
cancels the gesture. Inspector transactions disable waveform seeking.

The waveform scans only when the timeline is shown. One cancelable worker
publishes one value result; source switches discard stale results, and closing
the timeline cancels its preview work. A rescan button handles explicit refresh
of a local path. No new playback clock, device owner or decoder was introduced.

## Bounded data and reuse

The existing FFmpeg AudioDecoder streams stereo 48 kHz blocks from a FileBytes
lease. At most 4,096 min/max bins are retained, including zero and both channels
so anti-phase samples cannot cancel. When full, adjacent complete bins merge;
the final partial bin and total duration retain exact sample counts. Peak
storage is 32 KiB, plus fixed metadata and existing decoder/work-queue buffers.
The UI aggregates bins per visible column instead of drawing decoded samples.

Source size is limited to 256 MiB. A scan has a cooperative 120-second processing
budget checked between decoded blocks, not a hard deadline for blocked native
file calls. Cancel/error does not stop music playback. No unbounded PCM cache or
disk waveform cache is created; a reopened timeline rescans its current source.

We studied the old project's live waveform aligner and FFmpeg `showwavespic`.
The former handles rolling alignment, and the latter queues audio frames until
it can create the full picture, which conflicts with the long-song memory
contract. We reuse the validated FFmpeg decoder, first-party bounded executor
and Dear ImGui interaction/drawing APIs. No upstream source was copied and no
dependency was added. See [provenance](../../provenance/music_waveform.json).

## Verification

- `waveform`: sparse opposite-polarity transients survive repeated bin merges;
  discontinuous input is rejected. Every decoded sample of the 128-second,
  24 MiB original music fixture lies within its recorded bin. Exact duration,
  progress, cancellation, invalid source and scanner reuse pass.
- `waveform_ui_en-US` / `zh-CN`: actual ImGui input commits one seek at 96 seconds
  on release, performs no seeks during the drag, respects disabled edits, hides
  the previous waveform immediately on source change and discards stale scans.
- Existing timeline pause/restart/loop/curve undo tests pass.
- `large_soundtrack_studio_gpu`: real Studio bind/save/publish/clear/reopen and
  opening the timeline succeeds while the 197-instruction music-driven graph
  renders. The test asserts a nonempty overview within the 4,096-bin limit.
- USB Android `e2b3b128` runs the same portable waveform and full-song checks.
  This tests the shared module, not a mobile Studio UI or APK lifecycle.

Logs: `out/waveform-core-tests.log`, `out/waveform-ui-retest.log`,
`out/waveform-studio-tests.log`, `out/waveform-android-tests.log`.
Actual Studio capture:
`out/windows-release/large-soundtrack-studio/475847313621400/soundtrack-studio.png`.

This is the overview/locating part of music arrangement. Persisted cue intervals,
multitrack audio editing and gapless loops remain distinct unfinished work.
Android application installation remains blocked as recorded in the preceding
[large-song validation](large_music_packages_2026-09-08.md).
