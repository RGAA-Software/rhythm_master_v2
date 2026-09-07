# Android music application integration, 2026-09-08

The Android acceptance app now links the already validated vcpkg FFmpeg
6.1.1#11 LGPL profile from `out/vcpkg-media-android/arm64-android`. No FFmpeg
source build or shared vcpkg SDK upgrade was performed. The shared media,
FilePlayback, analysis, PlaybackClock and Player services provide decoding,
audio output, music features, graph time and video requests.

Android Java remains a document-picker, controls and audio-focus adapter.
It offers local music, the original demo PCM, pause/resume/restart, whole-track
looping and a position slider. Document imports run on one executor, are
bounded to 64 MiB for music and 16 MiB for runtime packages, and copy into
private cache. Native leases retain replaced music until the worker acknowledges
replacement; at most two music files are owned. Original documents are untouched.
Background suspension freezes playback. Audio-focus loss cancels automatic
resume; the user can explicitly resume later.

The default authored scene is the complete 164-node Resonance Gate package,
with the existing Balanced quality default. The app initially waits for the
user to choose demo/local music. No microphone, screen-audio capture or network
transport is introduced by this increment.

## Delivery and verification

- `out/android-arm64-release/apk/rhythm-player-release.apk`, approximately
  26 MiB. Packaging strips copies of native libraries; original incremental
  build/debug outputs remain intact. Dependency closure, package profile,
  16 KiB alignment, signing and FFmpeg/application hash matching pass.
- Companion `rhythm-player-release-relink.zip`, approximately 102 MiB, retains
  exact patched FFmpeg source, vcpkg recipe/manifest, actual flags, notices,
  application objects and link archives. Its Python relink command was run
  successfully using only enclosed inputs and NDK 29.0.14206865. The APK carries
  LGPL notices and identifies the companion materials. The project outbound
  license is still unselected; these are local acceptance artifacts.
- `out/android-music-host-tests.log`: the portable Android music-file adapter
  passes actual Windows audio tests for ownership boundaries, replacement,
  suspend/focus-loss behavior and destruction order.
- `out/android-music-media-tests.log`: the media-enabled Android build passes
  exact PCM/resampling/seek/cancel, RGBA/alpha/PTS/VFR/B-frame/rotation/embedded
  decoding, and all native GLES contracts on e2b3b128.
- `out/android-music-final-build.log`: incremental 20-worker build, source and
  relink packaging, Java compilation, signing and archive checks. No new C++
  compiler warnings. The retained tooling may emit existing upstream notices.

APK installation previously returned `INSTALL_FAILED_USER_RESTRICTED` from
the phone. Native command-line testing succeeds, but that does not validate
the Java/SDL audio device path. Actual app music output, document-picker flow,
focus changes, pause/resume, rotation, repeated background/foreground transitions,
long-run memory and subjective visual acceptance remain pending installation.
Do not mark Android or the broader product roadmap complete from this artifact.
