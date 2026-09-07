# Local media application integration

The Windows Studio and Player now share the same audio input panel and playback
service. Local music supports play, pause, seek on slider release, volume and
repeat. Switching to system capture stops file playback; selecting a music file
stops capture. Window suspension pauses the selected music and restores its prior
playing state. Audio analysis follows the bounded output-consumption estimate.
The normal Player also accepts `--audio <local-file>`.

Repeat drains the current device tail, then creates a new generation with cleared
analysis. It is not gapless looping. Seek still replays from the beginning; long
file indexed seeks and a unified authoring/media transport remain outstanding.
Music is selected locally; this increment does not embed the selected soundtrack
into a published graph or provide video graph nodes.

## Validated dependency profile

The shared vcpkg Windows installation remains unchanged. Its existing GPL FFmpeg
profile is not used by the applications. An isolated installation produced by
`C:/source/vcpkg/vcpkg.exe` provides FFmpeg 6.1.1#11 with avcodec, avformat,
swresample, swscale and zlib. No project-owned FFmpeg build script is used.
The matching zlib 1.3.1 override is required because this FFmpeg port maps `-lz`
to `zlib.lib`, while the later zlib 1.3.2 port renamed it `z.lib`.

Actual Release and Debug binaries report `LGPL version 2.1 or later`; their
configuration excludes GPL/nonfree/x264. Dynamic linking, notices and matching
source/build materials follow the [FFmpeg project guidance](https://ffmpeg.org/legal.html).
This does not choose the repository's outbound license or establish public-release
or application-store readiness.

- Manifest: `probes/media/vcpkg.json`; installed Windows SDK:
  `out/vcpkg-media-lgpl/x64-windows`.
- Exact binary configuration and hashes: `provenance/media_lgpl_windows.json`.
- `tools/prepare-media-notices.py` preserves the patched source tree and exact
  vcpkg recipe in `out/release-sources/ffmpeg-vcpkg.zip`.
- Every Windows app deployment carries 21 required DLLs, matching source/build
  archive and notices. The dependency copier prefers this validated SDK and
  checks matching DLL hashes, preventing accidental use of the old GPL DLLs.
- vcpkg manifest installations use separate roots for each target platform;
  switching a manifest root between triplets removes the other target's packages.
  Windows was restored from its binary cache after discovering this behavior.

## Video/image adapter

The private RAII decoder adapts FFmpeg's MIT `demux_decode.c` and `scale_video.c`
examples. Exact source bytes and notices are retained in
`provenance/ffmpeg_examples.json`. It returns owned, straight-alpha RGBA frames,
presentation timestamps, duration, sample aspect and rotation. The initial
profile is SDR, at most 4096 per axis and 2,073,600 pixels. HDR/wide-gamut and
unsupported matrices are rejected. This is software conversion, not a hardware
decode or zero-copy claim.

Local files and copied embedded bytes (at most 16 MiB) share a custom FFmpeg I/O
adapter. There are no temporary files or network protocols for embedded assets.
Probe decoders receive the pixel budget before stream inspection. Transactional
seek preserves the previous decoder on failure; EOF drains reordered frames.
Static decoded images now reach `texture.image`; video graph integration remains
in progress.

## Evidence and remaining device work

- `out/media-lgpl-video-tests.log`, `out/embedded-video-tests.log`: exact lossless
  RGBA, PNG alpha, VFR, B-frame drain, display rotation, seek, cancellation,
  malformed/nonlocal sources and owned embedded-image bytes pass on Windows.
- `out/embedded-audio-regression.log`: local-I/O extension preserves exact audio.
- `out/audio-repeat-tests.log`: real output at volume zero, repeated generation
  changes, pause, seek, EOF, bounded queue, cancellation and failure recovery pass.
- `out/windows-media-smoke.log`: both deployed apps start with the new DLL closure.
- `out/windows-player-audio-smoke.log`: actual Player receives music features and
  renders the layered-neon package for 30 frames, with test output muted.
- Existing Android FFmpeg 6.1#2 passes video timelines/RGB/B-frame/rotation checks
  but has no PNG decoder. A 16x12 MPEG-4 fixture also exposes its tiny-frame scratch
  buffer limitation; the shared normal-profile fixture is now 64x48. The isolated
  `out/vcpkg-media-android/arm64-android` API-26 profile now passes PNG alpha and
  copied embedded-video/image ownership on the USB device
  (`out/android-lgpl-video-tests.log`). It is not yet part of the Player APK.

Android audio still needs the actual Java/SDL application host and installation
acceptance. Neither native decoder tests nor a generated APK establishes those
results. Communications remain paused.

## Static images and background video selection

`texture.image` supports imported PNG/JPEG/WebP/BMP still assets, fit/fill framing,
orientation metadata, inline viewers and runtime packages. The focused
`prepared_assets` module composes existing GLB preparation with FFmpeg image
preparation. One existing bounded foundation worker atomically publishes a plan
and all CPU resources; stale completions cannot partially replace live assets.
The renderer shares one upload per asset ID and retains a 64 MiB decoded-image
budget. The existing 8 MiB package-asset limit still applies; decoding remains SDR
and at most 2 megapixels per image. Animated images are rejected for this node.

- `out/image-asset-integration-tests.log`: model/Player/content checks pass; the
  initial image test caught its incorrectly located fixture asset store.
- `out/image-closure-tests.log`: corrected image import/save/publish/Player,
  boundary checks and actual D3D11 fit/fill/alpha/device/resize checks pass.
- `out/image-app-smoke.log`: image and GPU checks plus both deployed apps pass.
- FFmpeg libraries now have explicit imported DLL targets and Debug/Release
  locations, so the existing Python post-build copier also deploys their closure
  for tests and the package compiler. Application deploy still verifies hashes.

The video-selection worker reuses the media adapter and the local-audio worker's
bounded demand/generation pattern. It selects the most recent PTS at/before the
requested time, holds through VFR gaps, supports source-duration loops and EOF,
and suppresses obsolete seeks. It retains current/lookahead/published frames and
one latest demand. Graph upload and timeline integration are subsequent work;
passing worker tests alone is not a video-node completion claim.

## Subsequent video graph integration

`texture.video` now consumes validated embedded clip assets through
`prepared_assets`, `video_sources` and the bounded playback workers. Nodes have
independent speed/offset/loop settings and share immutable encoded assets; at
most four instances are active. Frames update stable RGBA texture handles before
sampling. Fit/fill, rotation, seeking, pause, loops, save/reopen, publication and
Player/device recreation are covered by the integration tests.

Evidence: `out/video-player-tests.log`, `out/video-gpu-tests.log` and
`out/video-node-regressions.log`. Actual D3D captures distinguish timeline/seek
positions and verify stable texture allocation. Android native video-worker and
GLES upload tests also pass (`out/android-video-playback-tests.log`,
`out/android-media-render-tests.log`); the main APK does not yet adopt FFmpeg media.

This is a bounded SDR picture path: existing 8 MiB packaged asset total,
2-megapixel frames, four active instances and replay-from-start seeks. Embedded
video audio, large external clip streaming and the shared authoring/media clock
remain unfinished. Communications remain paused.
