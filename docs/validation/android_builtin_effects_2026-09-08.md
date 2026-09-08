# Android built-in effects and scene orientation, 2026-09-08

The Player now bundles the existing 32 authored examples in its APK. The primary
control is **Choose effect / 选择效果**, opening a native list with authored
thumbnails, localized titles, canvas orientation and music-input labels.
Selecting a built-in effect does not open Android storage UI. Import work remains
a secondary control for externally published works; music has local-file and
bundled-demo controls. These are existing examples, not 32 newly accepted designs
or completion of the 50 Basic / 50 Advanced quality target.

`tools/android_effects.py` packages the existing host-compiled runtime content,
generates catalog metadata from source manifests and runtime canvas dimensions,
and losslessly encodes the authored RGBA thumbnails as PNG using Python's standard
library. All catalog/package/thumbnail inputs participate in APK incremental
fingerprinting. Archive verification checks catalog identity and asset closure,
package/program/thumbnail hashes, canvas agreement, both title locales and native
dependency/alignment/FFmpeg provenance. Missing compiled content fails packaging
with the required `runtime_builtin_content` prerequisite rather than silently
omitting examples. No new effect engine or dependency was introduced.

The native Android `ListView`/adapter and `AssetManager` supply selection and
assets. One existing import executor loads the catalog and copies a selected asset
to private staging; the existing bounded asynchronous PackageImports/PackageLoader
path validates and atomically installs the work. The accepted Session supplies the
displayed title and orientation, so a failed load cannot advertise a new scene.
Selected works survive process restart and overlay installation.

Visual-only changes preserve current music, music position, loop setting and
pause state during the running session. A published work with a bound soundtrack
replaces the music. Independently chosen music and its position are not restored
after process termination by this change.

Orientation uses the accepted authored canvas: wide requests user landscape,
tall requests user portrait, and square allows user rotation. Quality and current
surface size never decide orientation. The host reuses the existing SDL
`SDLActivity.setOrientationBis` implementation in the retained SDL source
(`third_party/sources/sdl`, revision
`96292a5b464258a2b926e0a3d72f8b98c2a81aa6`, Zlib); no upstream source was modified.
Controls occupy a scrollable right panel in landscape and bottom panel in portrait;
the render surface uses the remaining area with aspect-fit presentation.

## Actual verification

- Incremental Release/20-worker native and Java build, APK signing, native 16 KiB
  alignment, all 32 catalog entries and matching LGPL relink materials pass:
  `out/android-builtin-effects-build.log`,
  `out/android-builtin-effects-final-build.log`,
  `out/android-arm64-release/apk/verification.json`.
- On USB e2b3b128 (Redmi K40S, API 34), `adb install -r` succeeded. No uninstall
  or app-data clearing was performed. The selected Music sculpture package SHA-256
  was identical before and after the final update:
  `82bc22afea03b38549c49006348dba6ce95d81eb5643eb681951227ccdae54a3`.
  Evidence: `out/android-effects-preservation.json`.
- Actual UI selection rendered Gate, Harmonic City, Sculpture particle echo,
  Music sculpture and Prismatic lotus. The catalog displays Chinese titles and
  thumbnails; City and Lotus rendered with real bundled PCM playback and nonzero
  analysis RMS. `out/android-effects-catalog.png`,
  `out/android-effects-city.png`, `out/android-effects-lotus.png`.
- Selecting Sculpture particle echo changed the display from 2400x1080 to
  1080x2400 with controls below and music continuing. Selecting square Music
  sculpture while paused retained 14.40/16.00 seconds and the paused state.
  Selecting Lotus returned to landscape. Evidence:
  `out/android-effects-portrait.png`, `out/android-effects-square-paused.png`.
- A process restart after overlay installation restored Music sculpture from
  private storage, shown in `out/android-effects-restored.png`.

The phone is left running the built-in Prismatic lotus with demo music looping.
This verifies the direct selection/orientation slice on this phone. It does not
establish subjective audio quality, gapless rotation, all 32 effects' device
visual acceptance, square-sensor behavior in both physical orientations, thermal
endurance, or completion of the wider Android/product roadmap.
