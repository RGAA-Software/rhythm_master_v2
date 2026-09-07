# Local media and audio output validation

Uses installed `C:/source/vcpkg` FFmpeg: Windows 6.1.1#11 (upstream n6.1.1),
Android arm64 6.1#2 (n6.1), as recorded by their SDK SPDX metadata. No FFmpeg
source is built. SDL3 remains the validated 3.2.20 build because SDL3 is absent
from this vcpkg installation; see `vcpkg_dependencies_2026-09-07.md`.

## Implemented behavior

- `src/media`: private RAII file I/O, demux/decode and 48 kHz stereo float
  conversion. Native filesystem access supports Unicode paths. Demuxer/protocol
  whitelists exclude playlists and nested/network protocol access.
- Each returned block has at most 4096 frames. Conversion staging is capped at
  1,048,576 frames, with additional decoded-frame/channel/rate/packet/probe/stream
  profile limits. These are not a total FFmpeg allocator limit or sandbox.
- EOF drains both codec and resampler. Exact sample-index seek replays from the
  beginning with bounded memory and cancellation; long-file indexed seek remains
  outstanding. Failed seek preserves the decoder. Mid-read failure requires
  seek/reopen, preventing incorrect timestamps on later samples.
- `src/audio_output`: thin SDL output, 0.5-second queue cap, pause/resume,
  clear, volume and EOF flush. Starts paused and owns no decoder or playlist.
- `src/audio_playback`: one worker owns decode/output/analysis, handles latest-wins
  load/seek/stop and publishes values. Bounded retained PCM feeds the canonical
  analyzer according to estimated presentation rather than decode-ahead.
  Generations reset analysis. Idle/paused workers wait for commands.

## Validation

`tools/build-media-probe.py windows|android` incrementally builds project code
against installed libraries with 20 workers. Windows matches the existing SDL
Debug runtime profile. Python automatically copies each probe, its DLL closure
and notices into a sibling `deploy` folder on each probe build.

`tools/test-media-probe.py --serial e2b3b128` runs the same decode contracts on
Windows and USB Android, then actual Windows output/playback tests. Exact binary
hashes/device directory are in `out/media-validation/results.json`; logs are
`out/media-test-*.log` and `out/audio_*-test-windows.log`.

Passed: exact stereo PCM, Unicode path, analytical 44.1-to-48 kHz tone accuracy,
partial blocks, EOF drain, repeated EOF, exact seeks, cancellation and invalid
input. A synthetic FLAC fixture encoded by the installed backend matches the
resampled WAV despite differing packet sizes. MP3/AAC/Vorbis/Opus presence is
known; their padding/duration/damaged-input behavior remains untested.

Actual Windows endpoint tests pass three lifetimes, bounded silent queues,
pause/resume, drain, clear and invalid PCM rejection. File playback tests pass
output consumption, canonical analysis, frozen paused clock, seek generation,
exact end position, superseding requests, stop and missing-file recovery.
Tests use volume zero; subjective listening, device switching and long-duration
audio/video synchronization are not implied.

Playback position subtracts the reported device buffer from device consumption;
it is an estimate, not a hardware timestamp. Android compiles the output/player
adapters, but actual audio requires the SDL Java host and APK acceptance.

The application's license decision and the phone's earlier APK installation
refusal remain pending. FFmpeg-dependent playback stays in the isolated target.
Main UI integration, indexed seek, seamless loop, mobile lifecycle, video and
export are outstanding. Main 45 Windows/36 Android suites and 41 presets/11
templates are unchanged by these checks. Steps 1 and 7 are not complete.
Communication remains stopped.
