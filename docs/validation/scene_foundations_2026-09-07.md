# Scene foundation and initial GLB import

Godot 4.5.1-stable is fixed at `f62fdbde15035c5576dad93e586201f4d41ef0cb`.
Projection equations and `SphereMesh::create_mesh_array` are adapted with MIT
notices; hierarchy, camera, material and glTF implementations are focused design
references. The engine is
not embedded. Exact inspected files/hashes are in `provenance/godot_3d.json`.

`scene3d` owns column-major transforms, right-handed +Y-up coordinates and -Z
camera forward. Canonical clip depth is [-1,+1]; graphics adapters must explicitly
adapt depth and target Y orientation. Numeric tests cover portrait/landscape,
perspective/orthographic near/far depth, hierarchy composition, inherited
visibility and inverse-transpose normals. Model validation bounds nodes (2048),
depth (64), meshes (512), materials (128), vertices (250000), indices (750000)
and mesh references (4096), including cycles and unordered parent records.

General vector, quaternion, TRS, matrix and inverse-transpose operations use the
existing vcpkg GLM 0.9.9.8#2 packages through a private adapter. No upstream math
types appear in project public APIs. `provenance/glm.json` records the installed
package identities and header hashes; the MIT alternative is selected and the
complete upstream license notice is retained. The shared vcpkg packages are not
upgraded for this change.

The Godot sphere extraction keeps its positions, ellipsoid normals, UV seam and
pole handling and triangle topology. It changes containers and math bindings,
reverses Godot's winding to the project's CCW convention, and checks dimensions
and tessellation before allocation. Hemisphere, tangent and UV2 generation are
outside this initial vertex contract. Tests independently check the analytic
ellipsoid equation and surface gradients, outward triangle winding, exact seam
closure and invalid/maximum tessellation limits.

The vcpkg cgltf 1.15 parser is compiled unmodified inside `model_import`; parser
types and allocation callbacks remain private. `tools/prepare-scene.py` prefers
installed headers and otherwise installs into the isolated `out/vcpkg-scene` SDK.
The initial importer accepts self-contained GLB 2.0 static triangle scenes with
one BIN buffer, positions, normals, UV0 and opaque scalar material factors.
Missing normals are generated; hierarchy and default scene visibility are kept.
Required extensions other than KHR_materials_unlit reject. External URIs,
textures, sparse accessors, compression, animation/skinning, morphs, cameras,
lights and advanced materials currently reject explicitly. These are profile
limits, not complete glTF support.

Input is bounded to 64 MiB, JSON to 8 MiB/depth 64 and native parser allocation to
a 64 MiB RAII arena. Accessor/stride/offset arithmetic is checked before native
index reads. Returned geometry owns its values; parser/input memory does not
survive the synchronous boundary. Tests cover imported geometry/materials,
hierarchy, ownership, malformed sizes/counts, URI/profile rejection and early
cancellation. Evidence: `out/model-import-windows.log` (two suites passed),
`out/scene-model-device-android.log` (the same two native suites on e2b3b128).

RhythmRender now owns mesh resources through move-only RAII values and stable
observer handles. Its scene pass uses bounded mesh/draw budgets, lazy depth
attachments, canonical projection remapping and explicit target lifetime. Windows
D3D11 pixel acceptance covers depth ordering and clearing, front/back/double-sided
faces, mirrored transforms and near/far clipping, with Scene -> Texture -> 2D
composition. These eight GPU cases and the math, primitive and import suites pass
in `out/scene-reuse-windows-tests.log` (four suites). The GPU fixture currently uses
triangles, not the imported GLB or the new sphere.

The subsequent [scene-node increment](scene_nodes_2026-09-07.md) connects procedural
Geometry/Material/Scene/Camera operators, node viewers, Godot-derived basic PBR and
directional lights to Studio and published Player packages. GLB asset binding and
background resource preparation remain integration work. The earlier foundation targets
cross-compile successfully for Android in `out/scene-reuse-android-build.log`.
Android cross-compilation does not substitute for the pending actual scene
GPU checks: USB device e2b3b128 was no longer visible when these tests were to be
pushed. APK installation also remains pending user/device authorization. Full
lighting, shadows, texture/material import and animation remain later increments
within the accepted task. The later validation record describes the adapted
Godot GLES BRDF's exact supported scope and remaining color-management work.
