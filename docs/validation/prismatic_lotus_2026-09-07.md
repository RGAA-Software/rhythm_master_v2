# Prismatic lotus: complex dynamic reference, 2026-09-07

Later metadata correction: see `template_title_encoding_2026-09-07.md`. The
original title was GBK-corrupted. Current packages/captures in the review page
have corrected UTF-8 titles; the hashes/capture paths below describe the earlier
visual implementation evidence, with identical graph/runtime program bytes.

## Delivered scope

One Advanced reference candidate, **棱镜星莲 / Prismatic lotus**, now runs through
normal graph compilation, Studio previews, publication and standalone Player.
It contains seven root nodes, an editable 24-node central composition, a two-node
dust component and a five-node glow component. The two independent grayscale
fields evolve, map into polar depth, fold into mirrored sectors, and become cool
and warm luminance contours. A soft aperture, counter-rotation, restrained RMS
breathing and two glow scales complete the composition. Silence keeps it moving.

The first captured version was too dark and dense. Actual captures were reviewed
and defaults were revised: fewer, stronger contours, distinct cool/warm layers,
scalar smoothing before contour extraction, and bounded looping rotation. Six
sampled time points show morphological development rather than rotation alone.
User artistic acceptance remains pending; the result is not counted as accepted.

Added general operators: `texture.mapping` (mirrored sectors / forward polar map)
and `texture.contours` (antialiased luminance isolines). Both have localized,
categorized editor entries and three native presets. Render shader creation and
binding moved into private `BgfxTexturePrograms`, which owns GPU lifetimes without
adding backend types to the public graph/runtime contract. No template-specific
renderer branch or rendered-video asset was added.

Current inventory: eight catalog semantic components, 95 native + 16 semantic
preset records, 25 runnable examples. Two Basic and one Advanced reference
candidates have new visual evidence; these are not 25 polished accepted templates.
The independent 50 Basic / 50 Advanced and 40 semantic-node targets remain open.

## Reuse and constraints

- Material Maker `addons/material_maker/nodes/kaleidoscope2.mmg`, revision
  `ad19fcf0ee34a7caf74df709dc4de7112f0d467d`: MIT angular replication adapted to
  mirrored sectors, aspect correction and bounded sampler coordinates. Seeded
  variation output is omitted. Actual upstream revision was read from the local
  reference checkout. Exact untouched source, license, hash and modifications
  are in `provenance/material_maker_effects.json`.
- TiXL `Operators/Lib/Assets/shaders/img/fx/PolarCoordinates.hlsl`, revision
  `fbc994d923e8a0142d2ff1b772e4d12248c5b0ba`: MIT forward polar mapping, radial
  power, twist and travel adapted to bgfx fragment uniforms. The center is bounded
  before negative powers; mirrored edge addressing is explicit. No inverse mode
  or upstream engine/API dependencies were imported. `provenance/tixl_effects.json`
  retains exact files, hashes and changes. Existing TiXL noise/blur passes are reused.
- Inspected TiXL's older `image-fx-kaleidoscope.hlsl`, which embeds a Shadertoy
  source reference. It was not copied; the selected Material Maker implementation
  has a directly recorded MIT license. The previously shortlisted dx11 shader is
  a wobble effect despite its name. Neither file is represented as an import.
- Inspected Material Maker `quantize.mmg` / `tones_step.mmg`: these emit per-channel
  quantization or a single threshold, not repeating luminance isolines with
  derivative coverage, premultiplied alpha and unresolved-line averaging. The
  small project-owned contour adapter supplies those missing behaviors.
- Original licenses, attribution and self-contained provenance JSON are included
  in both deploy notice trees. The project outbound license remains undecided.
  No new package, whole engine, FFmpeg application backend or networking module
  was adopted; vcpkg continues to provide the offline preview encoding tool.

## Verification and artifacts

Module increments were built/tested before application integration, using 20
workers and existing caches. `out/lotus-integration-build.log` records Studio,
Player, all content and sibling deploys. Each deploy contains 16 runtime DLLs.

`out/lotus-integration-tests.log` passed ten suites: render contracts, blur,
texture commands, template contracts, content presets, Windows effects GPU,
deployment smoke, shader compiler contracts, Studio smoke and Player smoke.
The source-boundary check initially lacked the extracted private adapter paths.
Its exemption was updated to exact renderer paths (instead of bare filenames),
retaining public API checks; `out/lotus-boundary-recheck.log` passed. Thus all
11 selected checks passed after correction; the original failure log is retained.

GPU pixel coverage includes the earlier four blur, five noise and resize cases,
plus constant-color mapping, quarter-turn symmetry with retained source detail,
negative-power polar extremes, contour spacing and transparent contour rejection.
Public contracts reject invalid bounds, fractional sector counts and multiple
shader effects in one command. Runtime binding/fallback/clamping tests cover the
new operators. Source, domain and render-layer dependency boundaries still pass.

Final actual Player capture directories:

- `out/lotus-preview/537fa99881f44ad7a29d9afbc87a3ac4`: 180 frames, 30 fps,
  960×540, deterministic synthetic audio features.
- `out/lotus-silent-preview/1d5723939d3e463180edb17da8189be8`: identical conditions
  with zero audio features. The default visual composition remains intact.

MP4, reference PNG, six-time-point contact sheet, silent clip, matching package,
hash records and benchmark log are collected in `out/effects-review/prismatic_lotus`.
`out/effects-review/index.html` presents them alongside the two Basic candidates.
The independent editable project is `out/effects-review/prismatic_lotus.rhythmproj`.
Studio opened it with seven of seven nodes/previews and rendered 30 smoke frames.
Publishing the reopened project produced a byte-identical package; standalone
Player opened that package and rendered 30 frames. Logs are
`out/lotus-studio-project-smoke.log` and `out/lotus-player-package-smoke.log`.
Package SHA-256:
`5fd52d128cef013d4ad9bad20e1a801b17dbab47b507f8b41c3e97b1c612f520`.

Both clips contain 180 changing, nonblank frames in the downsampled luma sanity
check (`motion-check.json`). This check does not establish flicker-free artistic
quality or actual microphone/system-audio capture. The six-second clips are
samples of continuous motion, not promised seamless six-second loops.

## Short performance sample and platform limit

`out/lotus-benchmark.log`: current Windows Debug D3D11 build, 1280×720; 120 warm-up
frames followed by 600 measured frames, no screenshots during measurement.
The machine reports an NVIDIA GeForce RTX 3060 and virtual display adapters.
Observed host frame time (including `EndFrame`/presentation pacing): median
16.6225 ms, p95 17.3271 ms. These are not isolated GPU timings or a guarantee of
60 fps. Texture allocation stayed at 105,769,988 bytes (about 100.9 MiB) throughout
the sample; final graph has 30 passes. This is short-run allocation evidence,
not a sustained-memory, thermal or mobile performance qualification.

`out/lotus-android-build.log`: shared runtime/render changes, native contract
executables, GLES 300 shaders and local Player APK all cross-built successfully.
An existing external Perl locale warning remains; no new project compiler warnings
were emitted. ADB listed no device. The previous installation restriction is still
unresolved, so Android visual/touch/lifecycle/performance acceptance is not claimed.
The APK's bundled default graph remains its existing sample; use an explicit new
package during future device acceptance. Apple remains the last platform stage.

Remaining visual work includes general displacement, controllable temporal trails,
the rest of the reference batch and content browsing/thumbnail UX. The 100-template
catalog and the rest of product steps 1–7 are incomplete. Communication stays paused.
