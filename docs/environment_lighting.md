# Environment lighting

R3 increment, 2026-09-08. Environment lighting augments direct lighting and
emission. It does not add a visible sky, local reflection probes, screen-space
reflections or HDR display output.

## Graph contract

`scene.environment` accepts a Scene and an equirectangular Texture, with optional
scalar intensity and rotation inputs. Intensity is 0–100, rotation is degrees
about world +Y, and source sRGB decoding is explicit. Linear Float16 sources may
retain values above one. Uploaded image/video sources use the existing media
and asset paths; there is no competing decoder.

One environment belongs to a scene. Attach it after merging objects; merging
two configured environments rejects ambiguity during compilation. A later
environment node explicitly replaces or disables the previous environment.
Scene transforms move geometry and lights but leave the world environment fixed.
The scene preview uses the authored environment without adding a fallback light.

## Filtering and rendering

The renderer prepares a linear RGBA16F atlas, 780×66 pixels (411,840 bytes).
Six horizontal tiles have 128×64 interiors and a one-pixel gutter on each edge.
Five GGX specular tiles use roughness 0, .25, .5, .75, 1; the sixth stores
cosine-weighted diffuse convolution. Each filtered pixel uses 64 Hammersley
samples. Longitude wraps; latitude clamps. Gutters prevent filtering across
unrelated roughness tiles. This is a bounded first IBL quality tier: tiny bright
sources and highly detailed mirror reflections need future higher-resolution
or mip-aware filtering. It is not a full Godot environment/probe system.

Source color is unpremultiplied and optionally decoded once before convolution.
Texture alpha does not represent a hole in the environment. A transparent black
pixel contributes black. Atlas output is opaque and linear; receiver energy and
the material are applied afterward. The receiver interpolates specular levels
and uses Godot's mobile environment BRDF approximation. Metallic suppresses
diffuse. ORM red occludes ambient diffuse/specular only; direct lights and
emission remain unaffected. Existing explicit SDR display/export nodes apply.

`EnvironmentPass` owns an atlas per scene renderer, including each demanded
scene preview. A stable source handle, source output version, color transfer and
presentation generation form its cache key. Changing intensity or rotation does
not refilter. Source input lifetimes are pinned through scene consumers. Hidden
previews, disabled environments and runtime reset release their atlases. Global
renderer texture and pass budgets continue to bound multiple active scenes.
Graph/scene APIs retain producer IDs and project texture handles; native sampler
objects stay private to the bgfx adapter. Invalid/stale/foreign handles, wrong
precision/extent, sampling the receiver target and non-finite settings reject.

## Reuse

Godot is the primary 3D reference. Its fixed 4.5.1 GLES scene shader supplies the
Lazarov environment BRDF approximation. TiXL's fixed local
`RenderToCubemap-vs.hlsl` supplies the GGX importance sampling, Hammersley sequence
and weighted prefilter loop. The latter targets D3D cubemaps/geometry shaders;
our existing cross-platform public texture contract exposes 2D textures. The
focused adaptation keeps those algorithms and replaces the projection/storage
with an atlas. The GLM piecewise sRGB conversion is shared with the existing
color pipeline. No new library or vcpkg package is introduced.

Exact revisions, file hashes, imported files, modifications and MIT notices are
recorded in [provenance](../provenance/environment_lighting.json). Existing
Godot/TiXL/GLM notices accompany deployed applications. This does not select the
repository's pending outbound license.

## Verification

Windows D3D11 and Android GLES 3.1 on e2b3b128 / Adreno 650 pass native GPU
readbacks for constant energy over every atlas pixel/gutter, sRGB decoding and
premultiplied input, metal/roughness response, diffuse, AO preserving direct
lighting, environment rotation and disabling. The same execution probe retains
the preceding instance/compute, float color, depth, local-light, material and
shadow checks. Logs: `out/ibl-render-tests.log`, `out/ibl-android-probe.log`.

Runtime tests pass on Windows and Android for animated rotation without refiltering,
changing source versions with a stable handle, source pinning, preview release,
disabling/reset and merge ambiguity. Windows scene/material/shadow/lifetime and
program-publication regressions also pass. The final native probes additionally
verify Float16 source radiance above one survives every atlas tile.

Sonic Enamel now contains 63 nodes and 82 edges, including a static procedural
studio environment whose rotation follows the existing time signal. Its bass,
treble and RMS controls remain editable. Real PCM comparisons at equal timeline
times have mean absolute RGB differences (0–255) of 2.80545 for demo/silence,
9.55097 for low/silence, 4.77508 for high/silence and 8.43789 for low/high. Repeated
120-frame MP4 export, timestamps/audio and cancellation checks pass. The actual
thumbnail is regenerated; visual acceptance remains pending the user's review.

Windows Studio/Player were incrementally rebuilt and each Python deploy directory
contains its executable/resources and all 20 required DLLs. Android was updated
with `adb install -r`, preserving data. The updated built-in effect was selected
in the UI; its packaged and selected bytes match:

- APK SHA256: `20e7a14033d35e5b9e1969d0665a60501dc0d5fc0e0dd67038993df6260bea89`.
- Sonic Enamel SHA256: `f1679a9f6ac997abd319d1d5e16fc47985564f37dfa3c0b0971e8154bb59e6d4`.
- Logs: `out/ibl-runtime-tests.log`, `out/ibl-runtime-android-tests.log`,
  `out/ibl-render-final-tests.log`, `out/ibl-android-final-tests.log`,
  `out/ibl-work-tests.log`, `out/ibl-delivery.json`.

Long-duration thermal/soak tests remain in final acceptance.
