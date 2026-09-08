# Shader host tool source build — 2026-09-09

The project now builds compatible shaderc 1.19.157 from a focused, hashed source
archive. The original project remains read-only. The vcpkg-first evaluation and
reproduction commands are in [the build guide](../host_shader_tool_build.md).

## Evidence

- Stock vcpkg bgfx tools 1.129.8940-496#1 builds successfully in the isolated
  project SDK. Its shaderc 1.18.129 produces FSH11 and fails the existing native
  image profile. It is retained only as a rejected candidate.
- The source fallback captures 1,881 files, including the exact eight original
  build configurations' source lists and required `.hpp11` headers. Archive:
  8,328,551 bytes, SHA256
  `272adaa5ed2807f22cc0918b6a5d60d7cffa6ac9ea20251a90060483eb0f1b23`.
- CMake/Ninja compiles 708 upstream translation units plus eight link targets
  with 20 workers, MSVC 14.51.36231, C++20/static CRT. The build command uses no
  original binary or original build directory.
- Rebuilt tool SHA256:
  `6f3d514d12edbde2e0055321a417e86091bc6ed0886f745abb4745ffcf08fa9a`.
- All 11 render groups for Windows SM5 and Android GLES 300/310, **48 programs**,
  reproduce identical bytes twice and match the legacy tool's output exactly.
  These include compute, skin/morph/instances, depth, environment, color and filters.
- Native image profile accepts both targets and rejects corrupted/unsupported
  containers. Native authoring passes diagnostics, cancellation, immutable
  replacement, save/reopen/publish/prepare. Invalid and stale compiler results
  preserve the existing valid artifact.
- Real D3D11 and USB Adreno 650 GLES image-program tests pass source alpha,
  top-left UV, parameters, high-precision time, cache and failed replacement.
- Windows Studio and Player incrementally build and deploy 20 DLLs plus resources.
  Studio carries the rebuilt tool, its selected profile, source inventory and
  original provenance/notices. Both platform build scripts select the validated
  project tool instead of inheriting the old checkout's path from cached settings.
- Three integrated CTests pass (image profile, shader runtime, Windows image GPU).
  Deployment smoke passes 20 local DLLs with system-only PATH, unrelated working
  directory and 30 GPU frames. The deployed compiler also passes actual Chinese
  Shader panel source entry, compilation, replacement and undo/redo interactions.
- Android reconfiguration uses the new host path. Embedded shader bytes and APK
  remain unchanged as expected. `adb install -r` succeeds; the existing Ink Tide
  selection and bound music resume (16-second timeline, RMS 0.0636079 in the
  captured frame). The current native GLES integration probe also passes compute,
  geometry/material/depth/animation checks. No long-duration claim is made.
- Running `python tools/build-shader-tool.py` again reports `ninja: no work to do`
  and the same compiler hash, confirming the normal incremental path.

Records: `provenance/shaderc_vcpkg_evaluation.json`,
`provenance/shaderc_source_build.json`, `provenance/shaderc_rebuilt_host.json`.
Logs: `out/r4-shader-source-final-build.log`, `out/r4-shader-rebuilt.log`,
`out/r4-shader-rebuilt/verification.json`, `out/r4-rebuilt-authoring.log`,
`out/r4-rebuilt-compiler-contracts.log`, `out/r4-rebuilt-windows-gpu.log`,
`out/r4-rebuilt-android-gpu.log`, `out/r4-rebuilt-windows-build.log`.
Integration logs: `out/r4-rebuilt-integrated-tests.log`, `out/r4-rebuilt-deploy.log`,
`out/r4-rebuilt-ui.log`, `out/r4-rebuilt-android-build.log`,
`out/r4-rebuilt-android-integrated.log`, `out/r4-shader-source-incremental.log`.
Phone image: `out/r4-rebuilt-phone.png`.

## Limits

Preparation corrections before the final build: missing `.hpp11` headers, and
CMake default exception flags/source charset. Three C4715 warnings remain in
unchanged upstream Tint: `intrinsic/table.cc:553`, `wgsl/resolver/resolver.cc:1345`,
and `ir/transform/decompose_access.cc:986`. They remain visible, recorded and
unsuppressed. No changed project C++ warnings or upstream source edits are hidden.

Repeated shader output is proven; cross-machine/toolchain/checkout-path
bit-identical **host executable** builds are not claimed. A source release must
include the locally retained source archive because it is Git-ignored. Existing
exact licenses, parser exception and per-file notices are retained. No outbound
project license is selected by this work.
