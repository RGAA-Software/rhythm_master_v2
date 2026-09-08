# Host Shader compiler source build

The Windows Studio compiler is a separate host executable. Android Player consumes
the compiled GLES artifact; it does not ship a compiler or compiler dependencies.
The graphics backend and the RMSH001/FSH12 image profile stay unchanged.

## vcpkg evaluation

Following the dependency preference, `C:/source/vcpkg` revision
`b216ddff25a1f432870e6c340ce79357049ef86e` was used to build
`bgfx[tools]:x64-windows-static-release` 1.129.8940-496#1. Installation,
build trees and packages are isolated under this project's `out/vcpkg-shader-*`;
the shared installed tree and FFmpeg packages were not changed.

The port successfully built shaderc 1.18.129. Its actual Windows and GLES output
uses container revision 11. Both the render compiler check and the existing
native image-program validator reject it (`shader.artifact_profile`). The
source also explicitly sets `BGFX_SHADER_BIN_VERSION 11`. It cannot replace the
validated shaderc 1.19.157 that generates revision 12. Changing the graphics ABI
or weakening package validation is not part of resolving this tool dependency.

The evaluated binary is not deployed. Exact package/source/patch hashes and the
repeatable isolated install command are recorded in
`provenance/shaderc_vcpkg_evaluation.json`. Original logs:
`out/r4-vcpkg-shader-install.log`, `out/r4-shader-vcpkg-candidate.log`, and
`out/r4-vcpkg-image-profile.log`.

## Focused source fallback

`tools/prepare-shader-tool.py --reference C:/source/shark_dynamics_wallpaper`
reads the original checkout only. It verifies the existing recorded eight build
projects and 709 source entries, checks selected files against the recorded Git
snapshot, then captures the release x64 source lists, defines, include paths,
headers and license notices. The excluded bx amalgamation stays excluded; 708
translation units are compiled. It does not import application code or prebuilt
libraries and does not change upstream tool sources.

The exact inventory is `provenance/shaderc_source_build.json`. The focused archive
is `third_party/source_archives/shaderc-1.19.157.zip` (locally retained, ignored by
Git). Source-release distribution must include that archive or supply the exact
recorded source snapshot; a Git checkout of this project alone does not contain
the ignored archive. The original upstream revision is still unknown; snapshot
revision and complete file hashes are retained rather than inventing one.

After capture, `python tools/build-shader-tool.py` verifies the archive and all
files, extracts into `out/shader-tool/source`, and invokes CMake/Ninja with 20
workers. Subsequent builds preserve source timestamps and incremental objects.
Only build configuration is generated as CMake; all preparation/validation stays
in Python. No access to the original checkout is needed for this build command.

The result is `out/shader-tool/build/shaderc.exe`. The build uses the upstream
Release x64 contract (static CRT, C++20, AVX, no RTTI/exceptions) and records its
binary SHA. Compiler/linker deterministic flags do not by themselves establish
bit-identical builds across toolchains, machines or checkout paths.

## Acceptance

`tools/verify-shader-tool.py --compiler PATH --output out/NAME` builds all 11
render groups twice for Windows SM5 and Android GLES 300/310. It checks 48
programs, exact repeated bytes and container versions. Its JSON explicitly
separates this evidence from native GPU execution.

The original compiler passes that complete baseline. Adoption of the source-built
tool additionally requires bounded image-profile/native authoring tests, actual
D3D11/GLES pixels, and deployment from the new tool path. Source-build and final
adoption evidence are recorded in the corresponding validation report.

Licenses and copyright notices remain those recorded in
`provenance/shaderc_host.json`, including the actual glslang parser exception.
This separate unmodified tool does not select an outbound project license.
