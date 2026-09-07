# vcpkg dependency sourcing check

User direction: prefer `C:/source/vcpkg` for all third-party dependencies and
build tools. This read-only check used `installed/vcpkg/status`, package metadata
and the project's actual CMake dependency declarations on 2026-09-07.
Only `Status: install ok installed` records count as available features.

| Dependency | Installed Windows / Android arm64 | Current action or concrete gap |
| --- | --- | --- |
| Protobuf | 6.33.4 / 6.33.4 | Already consumed from vcpkg; retain matching Windows host protoc for cross builds |
| nlohmann-json | 3.11.2 / 3.12.0 | Already consumed from vcpkg; package compatibility is covered by existing Windows/Android tests |
| FreeType | 2.12.1 / 2.14.3 | Windows already consumes vcpkg; Android installation is availability evidence, not a new UI dependency |
| FFmpeg | 6.1.1 / 6.1 | Use these packages for media validation; custom 8.1.2 build route stopped and its Python builder removed |
| GLM | 0.9.9.8 / 0.9.9.8 | Available first choice for planned 3D work; not adopted merely by this check |
| Dear ImGui | 1.91.9 / absent | Installed Windows features list DX11/DX12/GLFW/OpenGL3/Vulkan bindings; no installed docking, SDL3 or FreeType feature records. Validate a suitable vcpkg feature profile before replacing the existing 1.91.9b docking source integration |
| SDL3 | absent / absent | Installed Windows SDL2 2.26.5 does not satisfy SDL3 APIs. Evaluate the SDL3 port; retain validated SDL3 3.2.20 sources and matching Android Java host until migration passes lifecycle/package checks |
| bgfx / bx / bimg | absent / absent | Evaluate vcpkg bgfx and host shaderc together, including shader ABI and D3D11/GLES profiles. Preserve current validated source integration while this gap is unresolved |
| imgui-node-editor | absent / absent | Existing maintained source integration has accepted interaction fixes; preserve those through any future port/overlay migration |
| miniz | absent / 3.1.1 | Compare the Android package's exported functions/configuration against the current pinned cross-platform source adapter before migration |
| PicoSHA2 | absent / absent | Existing pinned header retained until an equivalent vcpkg package/profile is validated |
| Box2D, ImPlot, ImGuizmo, nativefiledialog-extended, cgltf, meshoptimizer, KTX | no matching installed records on either checked triplet | Candidates remain candidates. Evaluate vcpkg ports when their product module is reached; do not install all candidates in advance |

FFmpeg's Windows installation includes installed `gpl` and `x264` features;
Android's installed feature records list the six media libraries and no GPL
feature. The earlier binary/configuration checks remain necessary evidence:
do not label the Windows binary LGPL-only or choose the project's outbound
license by inference. Upstream notices and exact binary configuration must
follow the selected artifact.

Read-only Windows binary validation passed: the installed avutil, swresample,
avcodec and avformat DLLs load, and decoder lookup succeeds for PCM S16LE,
PCM F32LE, MP3, AAC, FLAC, Vorbis and Opus. Exact DLL paths, library versions,
runtime license strings and build configurations are recorded in
`out/vcpkg-media-windows-validation.json`. This checks binary loading and decoder
availability; file decoding, resampling, playback and Android execution remain
separate acceptance work.

The stopped FFmpeg source route's archive and existing build/install artifacts
are retained solely as unused references; they are not a fallback SDK or
application dependency. No shared vcpkg package was
installed, rebuilt, upgraded or removed by this check. No original project or
GammaRay repository was modified. Source/fork exceptions above are transition
records, not permanent exemptions from the vcpkg-first preference.
