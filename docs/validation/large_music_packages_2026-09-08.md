# Large music packages: Studio, Windows Player and USB Android

## Delivered behavior

Studio can bind, save, reopen and publish one music file up to 256 MiB. Small
works retain `music-performance-v1`; works whose music and ordinary assets exceed
8 MiB use the explicit `music-performance-v2` profile, with program ABI 2.
Ordinary assets retain an 8 MiB combined budget, at most 64 records including
music, and the file archive is capped at 272 MiB. The asset panel accounts for
bound music separately so it does not disable subsequent image imports.

The new ZIP profile contains one `media/<sha256>` STORE entry. Existing pinned
miniz validates ZIP structure, local headers and CRC; PicoSHA2 validates content
in 64 KiB chunks. A private allocator limits live miniz buffers to 2 MiB and 128
allocations, including simultaneous old/replacement buffers during reallocation.
Its bookkeeping has a bounded additional cost. This is not a whole-process RSS
limit. Ordinary program/assets and FFmpeg retain their separate existing budgets.

Player preparation retains an open file range instead of copying the song into
an archive string or an audio vector. The same FFmpeg adapter and audio worker
handle seek, pause, loop and replacement. Background publication/import validates
before atomic commit; failure preserves the previous package. A decoder holding
the previous file continues reading it across atomic replacement. FileBytes is
an open-file lease, not a snapshot of arbitrary in-place POSIX writes; callers
use immutable asset blobs or private imported files and atomic replacement.

The Android document picker copies packages in 64 KiB chunks up to 272 MiB.
Its standalone local-music picker retains its independent 64 MiB limit. Built-in
APK content retains the small profile; external packages use the file loader.
Native import owns one active and one replaceable pending file, and the current
decoder may retain a removed file until source replacement. Package disk usage
therefore includes staging/installed/retained files; it is not a single-file quota.

## Reuse

No new decoder, ZIP parser or dependency was introduced. The pinned miniz source
is unchanged. The private adapter relies on its public extraction iterator's
validated payload offset, so upgrading miniz requires rerunning the range and
CRC tests. See [provenance](../../provenance/large_music_packages.json) and the
[preceding file-range validation](file_audio_ranges_2026-09-08.md).

## Windows evidence

- `file_archive`: 20 MiB stored payload, direct range samples, CRC/hash corruption,
  malformed media metadata, old ZIP compatibility, oversized central-directory
  allocation rejection, cancellation, budget boundaries, failed installation and
  publication preservation, and retained readers across replacement.
- `large_soundtrack_player`: original 128-second stereo 48 kHz PCM fixture,
  **24,576,044 bytes**. Save/reopen, template copy, publication, background install,
  complete decoded PCM comparison with the original, seek and retained source
  after replacement all pass. The small FLAC profile also passes.
- `large_soundtrack_ui_en-US` / `zh-CN`: real ImGui bind/settings/clear/undo,
  save/publish and stale-worker completion checks pass with that large fixture.
- `large_soundtrack_studio_gpu`: actual Studio toolbar and inspector operations
  bind, save, publish, clear and reopen Resonance Live. Music resumes and the
  197-instruction graph remains valid without raising resource budgets.
- Deployed Windows Player opens that exact Studio package and observes real
  audio-driven GPU output during its 30-frame smoke check.

Logs: `out/large-music-seek-tests.log`, `out/large-music-studio-tests.log`,
`out/large-music-windows-player.log` and `out/large-music-integration-tests.log`.
Studio capture:
`out/windows-release/large-soundtrack-studio/474535399539900/soundtrack-studio.png`.
Published package:
`out/windows-release/large-soundtrack-studio/474535399539900/Published/音乐作品.rhythmpack`,
**24,581,263 bytes**. The fixture is generated from the project's original synth
loop; no external music rights are assumed.

## USB Android evidence and remaining gate

Device `e2b3b128`, 22021211RC/munch, Android API 34, Adreno 650. Incremental NDK
Release builds use existing vcpkg FFmpeg. Native ZIP and full-song Player PCM
tests pass (`out/large-music-android-core-tests.log`). The ZIP test's observed
native buffer peak is **10,080 bytes**, independent of its 20 MiB payload.

The exact Studio-published work runs through the shared file decoder, audio
analyzer, Session and actual GLES renderer: 240 music frames and 240 silence
frames at 640×360, excluding the first 60 frames from timing:

| Input | p50 | p95 | Stable texture bytes | Maximum RMS |
| --- | ---: | ---: | ---: | ---: |
| Music | 17.5666 ms | 19.1006 ms | 26,742,788 | 0.22358 |
| Silence | 17.4355 ms | 18.9395 ms | 26,742,788 | 0 |

Mean RGB difference is **9.93483 / 255**; music-image mean RGB is 17.71.
Evidence: `out/large-music-gles-tests.log`, phone directory
`/data/local/tmp/rhythm-large-music`. This does not establish stable 60 fps.

The updated APK and verified FFmpeg relink companion are generated under
`out/android-arm64-release/apk/`. The phone again rejects installation with
`INSTALL_FAILED_USER_RESTRICTED: Install canceled by user`
(`out/large-music-apk-install.log`). Actual picker, SDL device audio, focus loss,
background/surface recreation and endurance acceptance remain unverified until
USB installation is permitted. Native executables do not replace that gate.

Multitrack arrangement, gapless looping, 50 Basic + 50 Advanced quality content
and full Android application acceptance remain on the authorized main roadmap.
Apple stays last; communication stays paused.
