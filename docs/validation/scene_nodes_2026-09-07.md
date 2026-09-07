# Procedural scene nodes and Godot BRDF adaptation

The procedural 3D path now reaches Studio, portable graph/runtime, node viewers,
publishing and Windows Player. It adds geometry.cube, geometry.sphere,
material.unlit, material.pbr, scene.instance, scene.transform, scene.merge,
scene.camera, scene.directional_light and scene.render. Geometry, Material,
Scene and Camera are distinct port types. Existing document/program versions
remain readable; operators are stored by type key, not a serialized enum ordinal.

Sphere generation and projection reuse pinned Godot 4.5.1 source; vector,
quaternion and matrix operations use installed vcpkg GLM. The shader extracts
Godot's Lambert diffuse, isotropic GGX distribution/visibility, Schlick Fresnel
and default dielectric F0 paths. Source, symbol scope and changes are recorded in
`provenance/godot_3d.json`; MIT copyright, license and authors are retained.
The project supplies graph controls, stable resource identities, ownership,
backend bindings, coordinate adaptation and budgets rather than another engine.

Geometry is immutable and shared by scene instances. GPU uploads are keyed by
stable geometry identity/revision and pruned when no longer referenced. Transform,
material and light edits reuse mesh uploads. Runtime output versions are monotonic
across node type replacement, preventing an old downstream upload from surviving
a cube-to-sphere change at the same node ID. Reset and hidden previews release
mesh and depth owners. Renderer capability checks require 32-bit indices and a
supported D24S8 render target.

The procedural graph profile bounds source geometry to 250,000 vertices / 750,000
indices, each scene to 256 instances / four directional lights, intermediate
instance snapshots to 4096, and final scene rendering to 3,000,000 indices per
evaluation. Merge growth rejects before runtime allocation. Transform Euler order
is X then Y then Z; translation follows rotation and uniform scale. Camera uses
vertical field of view or orthographic height, with canonical depth conversion.
Lit normals use the GLM inverse transpose, including nonuniform model hierarchy
scales. Orthographic lighting uses parallel view directions.

Geometry, material and scene nodes have GPU previews, 256x144, 15 Hz, with at most
eight requested nodes. Isolated material/light previews use a sphere and neutral
preview lighting; this does not add lights to authored final output. Preview-only
branches retain the existing rate/culling policy. No routine GPU readback is used.

Validation:

- `out/scene-graph-tests.log`: four graph/binding/component suites passed.
- `out/scene-runtime-tests.log`: five scene/render/runtime/point suites passed.
- `out/pbr-render-tests.log`: render contract, real D3D pixels and shader tooling passed.
- `out/pbr-runtime-tests.log`: graph/runtime/math and published-program GPU cases passed.
- `out/pbr-integration-tests.log`: 38 selected integration suites passed, including
  deployment, project/package round trips, content, UI interactions and Player.
- The shared GPU fixtures now contain 13 renderer cases and nine graph/program
  cases: depth/culling/clipping, unlit, no light, front/back light, dielectric vs
  metal, emissive output, procedural geometry, transform, cameras and scene preview.
  Expected normal-incidence rough dielectric/metal pixels are 82/20 in the current
  linear render-target encoding, with tolerance two byte levels.
- `out/scene-studio-smoke.log` and `out/scene-studio.png`: actual Studio scene,
  output and inline previews; `out/studio-loaded-modules.txt` checked without Qt.
- `out/pbr-integration-android-build.log`: shared targets, GLES shader variants,
  native library and acceptance APK cross-build successfully. The Android fixtures
  contain the same 22 scene cases, but have **not** run on the disconnected device.

Five templates were added: Rotating cube, Ellipsoid duet, Two-light sculpture,
Music sculpture and Sculpture particle echo. The last composes Scene, Point,
Texture, time signals and feedback. All five Windows-published packages completed
30 actual Player GPU frames; logs are under `out/*-player-windows.log`. Content now
totals 82 presets and 22 templates, below the final 120/24 target.

The initial PBR path has opaque and object-sorted translucent draws, scalar
metallic/roughness/emissive factors and at most four directional lights. Roughness
is clamped to 0.05 for bounded numerical behavior. Intersecting transparency,
texture materials, point/spot lights, shadows, IBL, HDR/display color management,
skinning and animation remain incomplete. GLB parsing is validated independently;
asset binding, background resource preparation and application/package integration
were subsequently added in `glb_integration_2026-09-07.md`. This increment does not
complete all of step 4 or Android step 7.

Windows deployment is automatic and includes 15 runtime DLLs, content, locales
and retained notices. Current executables are
`out/windows/src/windows_spike/deploy/rhythm_master.exe` and
`out/windows/src/windows_player/deploy/rhythm_player.exe`. The Android APK is
`out/android-arm64/apk/rhythm-player-debug.apk`; no installation was retried.
