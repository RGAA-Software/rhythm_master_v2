# Static GLB assets in Studio and Player

This increment connects the previously validated cgltf importer to local assets,
graph authoring, publishing and Player. It does not complete the seven-step plan.

## Reuse and boundaries

Parsing continues to use unmodified vcpkg cgltf 1.15. Scene mathematics and rendering
reuse the recorded GLM/Godot implementation. Resource jobs use the existing bounded
GammaRay foundation executor and content-addressed asset store. New project code
only implements resource identity, preparation, budgets, cancellation and host
adoption; upstream engine resource loaders cannot directly satisfy our value-only
graph contracts and project package/CAS layout.

The real fixture is Cesium Box from Khronos glTF-Sample-Assets at
`9429648735279342b4c32b8745f7904196607379`, copyright 2017 Cesium, CC-BY-4.0.
`provenance/khronos_box.json` records exact files and hashes. The unmodified GLB,
source README/license, attribution and full license text are retained. The validation
project title credits Cesium and CC-BY-4.0; its standalone package embeds the notices
as text assets. This fixture is not counted as a first-party built-in template.

## Behavior

- `geometry.glb` selects an imported resource in the property inspector. References
  persist as typed lowercase SHA-256 values through project/program/package codecs
  and component properties, rather than machine-local paths.
- `model_assets` validates bytes and prepares immutable CPU models off the render
  thread. One worker and one replaceable pending request bound Studio loading.
  Cancellation and generation checks suppress obsolete results. Parameter-only
  changes reuse prepared resources without disk access or GLB parsing.
- Studio retains the last valid plan while loading or on failure. Asset removal and
  undo/redo also trigger validation. Geometry/scene previews use the same resources.
- Publishing checks model parsing and resolved geometry/draw budgets before replacing
  a package. Package encode/decode rejects missing referenced assets. Player validates
  model resources before replacing its current package; a failed load preserves the
  active frame. Surface recreation releases GPU owners but retains prepared CPU models.
- Existing package limits remain 64 assets, 8 MiB total asset bytes, 16 MiB archive.
  Scene limits include 250,000 vertices, 750,000 source indices, 3,000,000 rendered
  indices and bounded hierarchy/instances/draw calls. Source compilation can defer
  imported counts; resource preparation and runtime must supply resolved counts.

Supported input remains the initial self-contained static GLB profile: triangle
meshes, node hierarchy, positions/normals/UV0 and opaque scalar PBR/unlit materials.
Textures/images, skins, animation, morphs, sparse/compressed geometry, external
buffers, imported cameras/lights and unsupported extensions reject explicitly.

## Evidence and acceptance

- `out/model-loader-tests.log`: real Box decoding/counts, package roundtrip,
  shared geometry, corruption/missing/wrong-format checks and latest-request/cancel.
- `out/model-publish-tests.log`: package and persistence regressions.
- `out/model-player-lifecycle-tests.log`: embedded GLB, move-only prepared package,
  failed-model rollback and GPU resource recreation.
- `out/model-studio-tests.log`: content, canvas/component interaction and boundaries.
- `out/glb-integration-gpu.log`: 25 D3D pixel cases: existing 22 scene cases plus
  embedded GLB material override, GPU recreation and transformed offscreen output.
- `out/glb-integration-tests.log`: 45 selected Windows integration suites pass;
  communication tests are excluded.
- `out/glb-publish-smoke.log`: 11 instructions / four embedded assets.
- `out/glb-player-smoke.log`: 30 actual Player GPU frames.
- `out/glb-studio-smoke.log`: 11 visible nodes, eight inline previews, 30 GPU frames.
- `out/glb-studio.png`: inspected ordinary-window Studio capture after background
  model preparation, with the GLB, eight GPU previews and final composed output.
  A fixed early screenshot can capture the startup/loading state; it is not a
  reliable readiness signal. The smoke checks verify completion explicitly.
- `out/glb-integration-android-rebuild.log`: Android shared libraries, tests and APK
  cross-build after correcting a Clang range-loop warning in a changed codec test.
  The first build log contains that resolved failure; APK archive validation alone
  is not device acceptance. ADB currently lists no device; installation was not retried.

`tools/prepare-model-validation.py --output out/<fresh-name>.rhythmproj` creates an
isolated acceptance project using the existing rotating-cube graph and real GLB.
Current generated artifacts are `out/glb-acceptance.rhythmproj` and
`out/glb-acceptance.rhythmpack`. Pass them with Studio `--project` or Player `--package`.

Applications are deployed automatically with 16 DLLs, locales/content/notices:
`out/windows/src/windows_spike/deploy/rhythm_master.exe` and
`out/windows/src/windows_player/deploy/rhythm_player.exe`.
Android APK: `out/android-arm64/apk/rhythm-player-debug.apk`.
The built-in catalog is now 83 presets / 22 templates, still below 120 / 24.
