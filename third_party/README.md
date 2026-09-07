# Phase A dependency records

These are experimental inputs, not a release dependency lock or a project license.
No outbound Rhythm Master license has been selected. Imported sources retain
their copyrights and licenses; no third-party ownership was transferred.

`tools/prepare-dependencies.ps1` checks exact Git commits and refuses tracked
modifications. Its selected reference copies are checked byte-for-byte. It writes
`source_inventory.json`, which lists every copied reference file and SHA-256.
All source trees are under ignored `sources/`; the old project and GammaRay are
read-only references. Focused GammaRay first-party executor/joiner reuse is now
recorded separately in `provenance/gammaray_common.json`; it imports no Asio.

| Input / source | Exact experiment identity | Files used / destination | Terms |
| --- | --- | --- | --- |
| [SDL](https://github.com/libsdl-org/SDL) | `96292a5b464258a2b926e0a3d72f8b98c2a81aa6` (3.2.20) | Full checkout; upstream static Windows video/input build → `platform_sdl`; SDL renderer/GPU disabled | zlib, `sources/sdl/LICENSE.txt` |
| [Dear ImGui](https://github.com/ocornut/imgui) | `4806a1924ff6181180bf5e4b8b79ab4394118875` (1.91.9b docking) | Core four `.cpp` files, SDL3 and FreeType backends → `spike_imgui` | MIT, `sources/imgui/LICENSE.txt`; embedded notices remain in headers |
| [imgui-node-editor](https://github.com/thedmd/imgui-node-editor) | `021aa0ea4da13fed864bafb2a92d4c5205076866` | node editor/API, canvas and crude_json `.cpp` files → `spike_node_editor` | MIT root; canvas also states a public-domain/perpetual permissive grant in its file header; preserve both |
| [PicoSHA2](https://github.com/okdshin/PicoSHA2) | `161cb3fc4170fa7a3eca9e582cebd27cc4d1fe29` | `picosha2.h` → private `project_io`, `asset_store`, `player_core` | MIT, `sources/picosha2/LICENSE` |
| [bgfx](https://github.com/bkaradzic/bgfx), [bx](https://github.com/bkaradzic/bx), [bimg](https://github.com/bkaradzic/bimg) | Reference snapshot from `shark_dynamics_wallpaper` parent revision `118dbc811718836ffb4ca62eec7384605df5be32`; upstream commits unknown | Exact file list/hashes in `source_inventory.json`; amalgamated bgfx/bx and bimg image.cpp → private Windows D3D11 backend | BSD-2-Clause roots; embedded components below retain separate terms |

There are no local modifications to these source files. Project-authored build
configuration, RAII adapters and UI translation code live in `cmake/` and `src/`.
The ImGui tag object `52fe0a05...` is not its commit; preparation now verifies the
dereferenced commit above. No upstream fork fix was needed for this combination.

The current GPU adapter consumes bgfx's `vs_ocornut_imgui_dxbc` and
`fs_ocornut_imgui_dxbc` bytecode from `examples/common/imgui/*.bin.h`. Their source
shaders, varying definition and `examples/common/common.sh` are retained in the
reference inventory under bgfx's notices. Rebuilding these UI shaders with a
source-pinned compiler is still pending; binary use does not close the shader tool gate.

A separate Compute probe uses a read-only shaderc 1.19.157 candidate from the old
build (`cmake-build-qt6/generated/bgfx_tools/bin/shaderc.exe`), SHA-256
`6f310cf7091937aab1f1dc0b4bdafcc5c317bc789d0fc2b82f46a28e29f143b5`.
It produces new artifacts only under this project's `out/`; it is not copied
into or distributed with Studio. Compute execution and failed/stale publication
tests passed. Rebuilding the compiler from recorded sources, its embedded tool
dependencies and its redistribution remain separate pending work.

Embedded source records:

| Component | Retained source and exact notice | Current use |
| --- | --- | --- |
| Microsoft DirectX-Headers | `bgfx/3rdparty/directx-headers/LICENSE`, MIT | D3D11 build headers; selected tree hashes in source inventory |
| Khronos headers | `bgfx/3rdparty/khronos/**`, per-file Khronos permissive/MIT notices | Retained API headers; OpenGL/GLES backend disabled in this profile |
| RenderDoc API | `bgfx/3rdparty/renderdoc/renderdoc_app.h`, MIT, Baldur Karlsson | bgfx adapter header; no RenderDoc binary bundled |
| TinySTL | `bx/include/tinystl/LICENSE`, BSD-2-Clause | bx/bgfx internal containers |
| dtoa-benchmark / stringtofloat | Two complete MIT notices inside `bx/src/dtoa.cpp`, Milo Yip / Grzegorz Kraszewski | Embedded bx numeric conversion; no local changes |
| Catch2 | `bx/3rdparty/catch/catch_amalgamated.*`, BSL-1.0 | Retained reference cache only; not compiled into our targets |

## Existing SDK snapshot

`tools/record-dependencies.py` reads the existing vcpkg installation, including
installed feature dependencies, without modifying it. `sdk_inventory.json`
records package versions, port revisions, ABI identifiers and installed file
hashes. `notices/sdk/<triplet>/<package>/` retains supplied copyright and SPDX
source/build records. This captures the actual mixed-age SDK; it is not a claim
that a clean machine can recreate it from a single vcpkg baseline.

| Dependency | Windows x64 | Android arm64 | Use / terms |
| --- | --- | --- | --- |
| Protobuf | 6.33.4#2 | 6.33.4#2 | Private graph codec; BSD-3-Clause; host x64 protoc also builds Android content/schema |
| Abseil | 20260107.1#3 | 20260107.1#3 | Protobuf dependency; Apache-2.0 |
| utf8-range | 6.33.4 | 6.33.4 | Protobuf dependency; MIT |
| nlohmann/json | 3.11.2 | 3.12.0#2 | Private metadata adapter; MIT; version difference recorded, not silently unified |
| FreeType | 2.12.1#3 | Not linked | Windows ImGui fonts; choose FreeType License (FTL) for this experiment; full supplied dual-license text retained |
| Brotli / bzip2 / libpng / zlib | 1.0.9 / 1.0.8 / 1.6.39 / 1.2.13 | Not linked by this profile | SDK feature dependencies; exact MIT / bzip2-1.0.6 / libpng / zlib texts in each supplied copyright file |
| vcpkg-cmake / vcpkg-cmake-config | 2023-05-04 / 2022-02-06#1 | Host tools only | Build metadata/helpers; MIT notices retained |

Portions of this software are copyright © 1996–2022 The FreeType Project
(www.freetype.org). All rights reserved.

Windows reads Microsoft YaHei from the installed operating system for the local
text experiment. The font is not copied into this repository or the resource
bundle. The official template graph and locale catalogs are newly authored
project content. A distributable multilingual font selection remains pending.

The tested artifacts are local Debug validation executables, static shared-core
libraries and their test data. Production packaging still requires a clean
dependency lock, complete artifact notices/source mapping and a selected project
license. Apple/Android GPU and font integration are not established by these
Windows input records. See `docs/validation/phase_a_2026-09-06.md` for evidence.

The runtime ZIP adapter uses unmodified miniz 3.1.1 at commit
`d10b03cc73475af673df40f06e5cefd1d5f940d9`, MIT, from
https://github.com/richgel999/miniz. `miniz_source.json` records all ten imported
files and SHA-256 hashes; `notices/miniz` retains LICENSE and source copyright
lines. Project changes are the private bounded memory adapter and its CMake
target, not modifications to upstream sources. Windows and Android arm64 pass
ZIP/hash/limit/atomic-replacement contracts. Apple remains pending.
The current bimg target excludes image_decode.cpp (which embeds another miniz);
future image decoding integration must resolve symbols and ownership explicitly.

Android Player additionally compiles the already pinned SDL Java host sources,
unchanged, under SDL's retained zlib license. The project-owned PlayerActivity
adds native controls and document access. The GLES adapter uses the pinned bgfx
embedded ESSL shaders with the same notices as the Windows DXBC profile.
An APK build is not evidence of Android GPU correctness; real-device acceptance
is tracked separately in implementation_progress.md.

QR generation uses the unchanged C++ Nayuki encoder embedded in the pinned
GammaRay revision recorded in `qrcodegen_source.json`. Its exact upstream commit
is unknown; the parent commit and file SHA-256 values identify the imported
bytes. `python tools/prepare-qrcode.py` reads those Git objects without changing
the reference repository. The complete Project Nayuki MIT notice stays in both
source files and `notices/qrcodegen/LICENSE`. Our adapted wrapper has separate
first-party attribution in `provenance/gammaray_qr.json`. The `qr_generator`
target keeps encoder types private; no product license is selected by this reuse.

Native QR generation contracts pass on Windows/Android. Test images match
byte-for-byte; test-only zxing-cpp 2.3.0 in `out/qr-validation` independently
decodes 20 original/transformed image cases. ZXing is not linked or bundled in
Studio/Player by this test and has not yet been adopted as the phone scanner.

QUIC candidate sources stay separate from adopted application dependencies.
`quic_probe_sources.json` records MsQuic v2.6.1, quiche 0.29.3 and the separate
released OpenSSL 3.5.8 source with exact commits/file hashes. MsQuic local changes
are retained in `patches/msquic-probe.patch`; quiche's locked dependency graph,
20 Windows and 17 Android normal runtime dependency notices, including BoringSSL,
are recorded in `probe_locks`. Rust library attribution is bundled with the
private quiche experiment deployment. Neither candidate is linked into Studio
or Player. Platform/function evidence and remaining gates are in
`docs/validation/quic_candidates_2026-09-07.md`.

The isolated `probes/security` identity adapter uses the same recorded OpenSSL
3.5.8 release and retained Apache-2.0 license. Project-owned crypto/DPAPI adapters
do not import additional source. Probe deployments retain the OpenSSL and MsQuic
notices; application linkage and unified TLS-provider layout remain pending.

The local audio analyzer carries the Nullsoft BSD-3-Clause notice for the
MilkdropFFT reference chain in `notices/audio_fft/LICENSE.txt`. Exact old-project
and upstream revisions are recorded in `provenance/audio_analysis.json`;
projectM's other LGPL components were not imported by this increment.

Box2D 3.1.1 is built by `C:/source/vcpkg` using the project overlay in
`dependencies/vcpkg/ports/box2d` (port revision 1). The narrow rounded-distance
patch fixes sensor overlap separation below linear slop. The MIT license is
retained in `notices/box2d/LICENSE.txt`; vcpkg port files retain Microsoft's MIT
notice beside the overlay. `provenance/physics2d.json` records source, patch,
triplets and first-party adapter references. No native physics types enter the
graph or runtime public APIs.

The initial 3D camera module adapts projection equations from Godot 4.5.1-stable,
commit `f62fdbde15035c5576dad93e586201f4d41ef0cb`. MIT notices and contributor
credits are in `notices/godot`; focused adaptation versus source study is recorded
in `provenance/godot_3d.json`. No Godot engine runtime or embedded third parties
are linked. The isolated static GLB importer uses unmodified vcpkg cgltf 1.15;
its MIT notice is in `notices/cgltf`, with exact hashes in `provenance/cgltf.json`.
# GLM mathematical operations

`scene3d` uses the installed vcpkg GLM 0.9.9.8#2 headers privately. The MIT license
alternative is selected; the full upstream notice is retained in
`notices/glm/LICENSE.txt`. Package identities, exact source revision and installed
header hashes are in `../provenance/glm.json`. No GLM sources are modified.

The GLB integration acceptance fixture is the unmodified Cesium Box from
Khronos glTF-Sample-Assets, revision `9429648735279342b4c32b8745f7904196607379`.
It is copyright 2017 Cesium, licensed CC-BY-4.0, not first-party content.
Source hashes are in `provenance/khronos_box.json`; model attribution and full
license text are retained under `notices/khronos-box`. The acceptance-project
generator embeds those notices as assets so standalone publishing retains them.

TiXL effect shaders are now reused in the private render adapter: five-sample
Gaussian blur, four-sample downsampling and a bounded four-octave Perlin field.
The exact upstream originals are retained in `sources/tixl-effects`; revision,
hashes and adaptations are in `../provenance/tixl_effects.json`. TiXL's MIT license
and attribution are bundled under `notices/tixl-effects`. No TiXL C# host, engine
or unrelated shader includes are embedded. The data-authored template graphs
remain first-party content. The shared vcpkg FFmpeg executable is used only as an
offline preview-encoding tool here, not linked into Studio or Player.

### Polar and mirrored sector mapping

TiXL `PolarCoordinates.hlsl` and Material Maker `kaleidoscope2.mmg` are retained
unchanged under `sources/tixl-effects` and `sources/material-maker-effects`.
The focused bgfx adaptations preserve MIT notices, with exact revisions, hashes
and modifications in `provenance/tixl_effects.json` and
`provenance/material_maker_effects.json`. No whole engine or Godot API was imported.

### Full Noto CJK font

`assets/noto-cjk/NotoSansCJKsc-Regular.otf` is the unmodified full upstream font
from notofonts/noto-cjk, revision `f8d157532fbfaeda587e826d4cd5b21a49186f7c`.
Copyright 2014–2021 Adobe. SIL Open Font License 1.1 and exact provenance are retained
in `notices/noto-cjk` and `provenance/noto_cjk.json`. The font is retained only as an unused reference after the user restored Microsoft
YaHei. New deployments no longer copy this font; retained notices cover older
local bundles that may still contain the experiment. No Windows system font is copied.
