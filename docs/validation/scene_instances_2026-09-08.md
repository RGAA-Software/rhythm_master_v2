# Scene instance batching and Spectral Foundry — 2026-09-08

R1 first functional increment delivered. GPU particle simulation remains pending;
this increment packs CPU-authored transforms for actual hardware instancing.

## Authoring and execution

`scene.point_instances` appears in the 3D node category, with Chinese/English
labels. Connect geometry, points, optional material and scale/height/audio-gain
signals. Canvas-normalized X/Y maps to world X/Z; point size and rotation control
horizontal size and Y rotation. Audio bands selected by point X stretch the Y
scale; each object center moves by half its height to keep cube columns grounded.
Missing/invalid audio contributes zero. Point colors can override/tint the material.
Capacity is explicit: excess points reject, rather than disappearing silently.

The graph compiler, runtime and package loader bound scene records to 16,384,
intermediate instance copies to 65,536, and rasterized indices to 3,000,000.
Unique geometry budgets stay unchanged. Ten thousand cubes fit; ten thousand
ordinary high-resolution spheres do not. Imported geometry gets checked again
with resolved mesh counts before GPU allocation. Older Players reject the new
unknown operator; existing source/package formats remain readable by this Player.

The native adapter batches consecutive compatible opaque records. Each 144-byte
record carries model, inverse-transpose normal matrix, and color. Different colors
can share a batch. Mesh/material/culling differences split batches; transparent
records retain their established order and individual draws. Unsupported
instancing or exhausted transient capacity uses the ordinary path. Capacity is
checked before scanning a group, avoiding repeated long scans when the buffer is
full. GPU handles and transient borrowed memory stay inside the render adapter.
FrameStats.draws now counts actual scene submissions.

`content/templates/spectral_foundry` is an editable 33-node, 36-edge composition:
one grid generates 4096 columns, plus a floor and three torus orbits (4100 instances).
Existing PBR, lights, blur and composite nodes form the picture; it is not a
hardcoded demo renderer. The scene has actual source, layout, localized catalog
metadata, rendered thumbnail, published package and Android direct selection.
It is a functional example; final visual-quality acceptance remains pending.

## Short verification

Seven pixel comparisons on D3D11 and Adreno 650/GLES 3.1 compare batching with
equal geometry under alternating distinct mesh handles, forcing ordinary draws:
per-instance colors, nonuniform lit normals, mirrored models, mixed materials,
double-sided models, overlapping transparency and culling-sign splits. Pixels
match within the unchanged two-byte numerical tolerance; draw counts are checked.

One captured submission at each scale (not an FPS benchmark or GPU timing):

| Records | Windows CPU validate/submit | Android CPU validate/submit | Draws | Instance bytes |
| --- | --- | --- | --- | --- |
| 1,000 | 0.1047 ms | 0.698177 ms | 1 | 144,000 |
| 10,000 | 0.693 ms | 2.555 ms | 1 | 1,440,000 |

Both devices produce 3,496 / 33,856 visible pixels in the corresponding 256-square
fixtures. These simple unlit grids do not establish throughput for every scene.

- Windows: scene_graph, point_instances, scene_runtime, template_contracts,
  program_contracts, windows_gpu_execution_probe and windows_scene_gpu pass.
- Android: point_instances_tests, instance pixel/scale probe, full existing GPU
  suite with device recreation, and 60 frames of the published new package pass.
- Decoded PCM via existing FFmpeg/analyzer, same scene time: mean RGB differences
  on the 0..255 scale are demo/silence 0.93876049, low/silence 4.29266999,
  high/silence 3.95745515, low/high 0.97727214. All exceed the existing 0.15 threshold.
  `spectral_foundry_music_gpu` reuses the music fixture and pixel checker.
- Synthetic canonical features were used separately for reproducible thumbnails
  and visual iteration; those captures are not claimed as live music evidence.
- Phone UI: new first-row effect thumbnail/title, landscape canvas, demo playback,
  nonzero RMS and loop playback observed. Overlay install preserved the previously
  selected Prismatic Lotus before selecting the new effect. No uninstall/data reset.

Local evidence: `out/point-instances-windows-tests.log`,
`out/point-instances-windows-probe.log`, `out/point-instances-android-android_gpu_contract_tests.log`,
`out/point-instances-android-point_instances_tests.log`,
`out/spectral-foundry-android-regression.log`, `out/spectral-foundry-music-tests.log`,
`out/windows-release/spectral-foundry-music-captures/pixel-differences.json`,
`out/spectral-foundry-android-picker.png`, `out/spectral-foundry-android-playing.png`.

## Delivery and reuse

Windows bundles are `out/windows-release/src/windows_spike/deploy/rhythm_master.exe`
and `out/windows-release/src/windows_player/deploy/rhythm_player.exe`, each with
20 DLLs plus resources copied by the existing Python deployment hooks.
Android `out/android-arm64-release/apk/rhythm-player-release.apk` is 27,640,607 bytes,
SHA256 `f5cde793e10e39039f8cf99e240d0e6fbc1bb640dfff933f815d8768562a58ed`.
Installed using `adb -s e2b3b128 install -r`. Contains 33 selectable effects.
The phone uses the explicitly validated GLES 3.1 build profile; fresh CMake default
remains GLES 3.0 until the broader supported-device policy is settled.

Spectral Foundry package SHA256:
`e5176ded558bd936ef914d44af74c3cc2211b6faccc180f84618fffc4b7f747b`, also checked in
Android private `files/selected.rhythmpack`. Final source authoring script is
`tools/author-spectral-foundry.py`; all dependencies remain the existing packages.

Reuse: existing bgfx instance APIs/shader macros (BSD-2-Clause, snapshot provenance
in [R0](gpu_execution_2026-09-08.md)); existing project point/scene/GLM adapters for
coordinate conversion, transforms and ownership. No upstream source changed.
The new composition reuses the existing first-party graph writer and attributed
Godot/TiXL rendering adapters. GPU particle source study follows separately;
none of TiXL's particle implementation is imported in this increment.

No long soak or thermal test was run. R1 GPU particles, R2–R6 and final quality
and long-duration acceptance remain outstanding.
