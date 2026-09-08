# GLB material images

R3 functional increment, 2026-09-08. `geometry.glb` now preserves supported
embedded material images in saved/published works. Studio still uses its existing
asset import and resource worker; Player consumes the same self-contained GLB
asset. There are no external file or network image lookups.

## Supported profile

- Static GLB 2.0 triangles and opaque unlit/metallic-roughness PBR materials.
- Embedded PNG/JPEG buffer views; UV0; repeat addressing and bilinear sampling.
  An omitted sampler uses this profile. Explicit mipmapped/nearest/clamped
  samplers, KTX/WebP extensions, UV1+, and texture transforms reject rather than
  silently rendering different materials.
- Base color and emission are sRGB; tangent normal and ORM are raw data. Material
  factors remain linear. OPAQUE ignores image alpha, including RGB stored at zero
  alpha. Decode produces straight RGB and forces alpha to 255 before upload.
- Normal scale is retained within the renderer's 0–4 range. A supplied valid
  tangent is retained; missing tangents use existing MikkTSpace generation when
  the normal map is needed. Missing UV0 on a textured primitive rejects.
- glTF metallic-roughness uses G/B. Its unused red channel is not treated as AO.
  Occlusion red and strength are combined into a prepared ORM image; absent AO
  becomes 1 and absent metallic-roughness becomes G=B=1. Separate AO and MR images
  must have identical dimensions in this profile. No implicit resizing occurs.

Alpha blend/mask textures, arbitrary sampler modes, texture transforms, additional
material lobes and full glTF animation remain separate capabilities. This is a
defined supported subset, not a claim of complete glTF or Godot material parity.

## Ownership and limits

The existing cgltf 1.15 vcpkg extraction is unchanged. Its buffer/image/texture
views remain inside `model_import`. The existing FFmpeg `VideoDecoder` performs
all decoding synchronously on the preparation worker, with cancellation; no
second decoder is introduced. Builds without media retain geometry-only import
and reject embedded images explicitly.

Each imported model owns decoded `TextureImage` values and material image indices.
No parser or FFmpeg types, file paths or encoded GLB byte references reach
Scene3D/Runtime. `ScenePass` owns lazy GPU uploads under its existing immutable
geometry identity/revision cache. All material slots referencing one image share
that upload. Graph texture producer IDs take precedence over an embedded slot;
an explicit graph material override replaces the imported material.

Limits include 64 source images, 128 textures/samplers, 16 MiB per encoded image,
4096 per dimension and 2,073,600 pixels per decoded image. Original decoded images
and any combined ORM images share the 64 MiB CPU image budget, across all models
and ordinary graph image assets in a prepared resource set. At most 192 prepared
images belong to a model. GPU uploads use the existing global texture budget;
geometry/scene limits remain unchanged. Hiding a preview, replacing a geometry
revision or resetting Runtime releases the associated uploads.

Read-only source references: Godot 4.5.1 `modules/gltf/gltf_document.cpp` material
channel/sampler handling and the installed cgltf header/accessor helpers. Actual
parsing remains cgltf; adaptation stays in project value/RAII adapters. Revisions,
hashes, MIT notices and changes are in [cgltf provenance](../provenance/cgltf.json)
and [Godot provenance](../provenance/godot_3d.json). FFmpeg retains the validated
vcpkg profile and distribution notices. No dependency version or outbound license
has been changed.

## Checks

Windows and USB Android pass embedded PNG decoding, RGB at zero alpha, material
slot/normal strength preservation, ORM channels/strength, UV0, external-image
rejection and unsupported sampler/UV profiles. Existing geometry bounds, malformed
GLB and cancellation tests remain active. Runtime tests cover upload sharing,
animation reuse, textured geometry previews and resource release. Asset preparation
and prior material/environment tests also pass.

The native GPU probe creates a hash-identified GLB asset, encodes/decodes a runtime
package, prepares it through the normal model/FFmpeg path, evaluates `geometry.glb`
through scene rendering and reads actual pixels. The tested center pixel on
Adreno 650/GLES 3.1 is RGB **57,69,84**; D3D11 passes the same expected range.
This catches missing images, material bindings, UV interpolation or geometry.

Logs: `out/glb-images-import-tests.log`, `out/glb-images-import-android-tests.log`,
`out/glb-images-runtime-tests.log`, `out/glb-images-runtime-android-tests.log`,
`out/glb-images-gpu-tests.log`, `out/glb-images-gpu-android-tests.log`.
Windows Studio/Player were incrementally rebuilt; both deploy directories include
all 20 DLLs and resources. Android was updated with `adb install -r`, preserving
its existing selected effect and app data. APK SHA256:
`941012850803afd9df098449485274522fe94075c4b1dd5867f4544b934e2cbe`.
Build/delivery logs are `out/glb-images-deploy-build.log`,
`out/glb-images-android-apk-build.log` and `out/glb-images-delivery.json`.
Long-duration acceptance is deferred until the remaining planned functionality
is complete.
