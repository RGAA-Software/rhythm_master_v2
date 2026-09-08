# Rhythm Master Next

Current development plan: [rendering capabilities and next deliveries](docs/rendering_capability_roadmap.md)
and [authoring feature gaps](docs/feature_gap_review_2026-09-08.md). Features and
editable music-driven works come first; long-duration tests are deferred to final
integrated acceptance. Android follows each supported shared-feature increment.

R1 now includes hardware scene batches and typed GPU particles, editable Spectral
Foundry/Nebula works, node previews, real music and MP4 checks on Windows, and
Android built-in playback. [Evidence and limits](docs/validation/gpu_particles_2026-09-08.md).
R2 float/color conversion, explicit sampled depth and music-driven DOF now pass
Windows/USB Android checks; [R2 evidence](docs/validation/float_depth_2026-09-08.md).
R3 now adds texture materials, point/spot lights and the editable Sonic Enamel work,
with Windows/Android delivery and real PCM/MP4 checks. [First R3 increment](docs/validation/material_textures_2026-09-08.md).
Single-light directional/spot shadows are also implemented and verified on both backends;
[shadow contracts](docs/shadow_pipeline.md). [Environment lighting](docs/environment_lighting.md)
now adds cached GGX reflections/diffuse on both platforms.
[Embedded GLB images](docs/glb_material_images.md) also pass package/GPU checks and
Windows/Android delivery. R3 functionality is closed within its documented profile;
R4–R6 and visual/endurance acceptance continue.
R4 starts with [ordered paths and tube geometry](docs/path_geometry.md), live path previews
and the editable music-driven Aurora Braid work. [GPU twist/taper](docs/mesh_deformation.md)
now shares mesh/image uploads across music changes and includes Torque Garden.
[Image shader authoring](docs/image_shader.md) now supports dual-target compilation,
immutable hot replacement, undo/redo and the editable Phase Loom work on Windows/Android.
[Model animation](docs/model_animation.md) now includes GLB node tracks, unified time and
music-driven clip mixing with shared uploads. 48-bone GPU skinning passes Windows/Android
checks. Four-target GPU morph, weight animation/music controls and the editable
Crystal Choir now also ships on both platforms, with real PCM and MP4 checks.
R4 functionality is delivered within the documented profiles; R5/R6 continue.
[The host shader tool now builds from a pinned source archive](docs/host_shader_tool_build.md)
inside this project. vcpkg's current tool was tested and rejected for container-version
incompatibility; all 48 shaders from the rebuilt compatible tool match the prior bytes.
R5 adds [public macros and parameter snapshots](docs/public_controls.md): Studio history,
Windows/Android live controls, manual A/B blending and an 86-node Crystal Choir with
four exposed controls. [Snapshot cue arrangement](docs/cue_arrangement.md) now provides
named timeline cues, interrupted fades, beat snapping and seek/export consistency.
[Video clip source intervals](docs/media_clip_arrangement.md) add trimmed, faded,
overlapping video tracks with timeline editing and shared FFmpeg playback/export.
[Multitrack audio arrangement](docs/audio_arrangement.md) adds shared playback/export PCM,
editable audio clips and the built-in Luminous Concerto media performance.
[Scene preparation and transitions](docs/scene_transition_plan.md) now provide a bounded
performance queue and shared-clock two-scene fades on Windows and Android.
R6 [asset maintenance](docs/asset_maintenance_plan.md) adds reference replacement/undo,
background integrity checks and original-file recovery through Studio to published Player.
[Component browsing and node help](docs/catalog_authoring_plan.md) provide live previews before
insertion and offline typed connection guidance. [Preview groups](docs/preview_navigation.md)
make every visible demand reachable within the shared eight-preview budget.
The independent content quality targets continue.
[Porcelain Pendulum](docs/validation/porcelain_pendulum_2026-09-09.md) adds a ceramic/brass
music work with editable cues, bound music, Windows export and Android built-in playback.
[Android catalog search](docs/android_catalog_navigation.md) filters bilingual names/descriptions,
template levels and canvas orientation before direct playback or queue insertion.
[Ink Tide](docs/validation/ink_tide_2026-09-09.md) adds an editable music-driven pigment,
coastline and contour composition with desktop export and Android selection.
[Input-processing components](docs/validation/input_components_2026-09-09.md) now preview
with demonstration inputs and insert only the editable component. Flow glass adds
music-driven refraction for the author's own source texture.
[Four further processing chains](docs/validation/processing_components_2026-09-09.md)
add prism folding, contour engraving, motion history and dual-radius glow; the catalog has 22 components.
[Resonant Arcade](docs/validation/resonant_arcade_2026-09-09.md) uses an embedded editable
Prism fold component on musical architectural screens, with cues, desktop export and Android playback.

Large-song authoring now supports one bound music file up to 256 MiB through
`music-performance-v2`, with streamed decoding in the shared Player core.
Windows Studio/Player and USB Android native PCM/GLES validation pass; Android
APK overlay installation and built-in effect selection now work on the USB phone;
broader Android lifecycle/endurance acceptance remains in progress. See
[Android built-in effects](docs/validation/android_builtin_effects_2026-09-08.md) and
[large-song evidence](docs/validation/large_music_packages_2026-09-08.md).

> Status: Phase A architecture validation and the first Windows slice are authorized.
> A Windows Studio slice and portable core are implemented and tested.
> 2026-09-07: the user accepted the current Windows build, including deployment,
> node dragging, pan cursor and node styling. Apple ports are deferred to the final platform stage.
> Phase A remains in progress; platform and graphics risk gates are not all closed.
> Latest product review rejects the flat node palette and current template visual
> quality. Earlier acceptance was limited to the interaction/deployment slice.
> Catalog counts are runnable examples and preset records, not polished content
> acceptance. See [quality correction](docs/validation/authoring_quality_correction_2026-09-07.md).
>
> Product scope, refined 2026-09-08: Rhythm Master targets a **real-time node-based
> visual authoring product in the class of TiXL and TouchDesigner**, with music
> visualization as its core use case. Desktop wallpaper hosting is outside its scope.
> [Product scope and acceptance](docs/product_scope.md) governs the roadmap.
>
> Decision date: 2026-09-06

This directory contains the Rhythm Master music visualization design and implementation.
Its core workflow is open-ended visual node authoring, reusable components,
audio/signal/parameter/time control and synchronized live node/final previews,
windowed/fullscreen playback, media export and portable package publishing. The new
application will not link Qt. Studio/node editing targets Windows and macOS;
the shared Player targets Windows, macOS, Android and iOS. It will retain the
proven portable rendering, graph, effect, audio-analysis and physics work from
the existing project, while rebuilding the application shell and editor around
SDL3, Dear ImGui, imgui-node-editor and RhythmRender/bgfx. Studio and Player are
multilingual products, initially shipping complete Simplified Chinese and
English locales.

Multi-platform architecture applies from the first shared module: Studio targets
Windows/macOS and Player targets Windows/macOS/Android/iOS. Windows-first is the
host delivery and product acceptance order. Shared contracts, package formats,
toolchain probes and dependency evaluation cover all target platforms now;
Android Player host development is now authorized and in progress; native
validation and an assembled APK do not establish full application acceptance.
Delivery order is Windows, then Android Player, then the final Apple platform
stage (macOS Studio/Player and iOS Player). No Apple hardware is currently available;
Apple toolchain, GPU and device validation move to that stage and do not block
Windows/Android development. Portable contracts and Apple compatibility review
remain required now; Apple support is recorded as deferred, not validated.

Cluster playback is planned: a desktop Host coordinates N QR-joined native
Players on a reachable venue LAN. Phones render locally from shared packages,
audio features and session time; video streaming is not the default. GammaRay
first-party executor/joiner and QR wrapper have been extracted with provenance.
Clock and bounded input models, runtime role/control/time nodes and an offline
Studio input panel are tested. Authenticated room transport, camera scanning
and measured venue capacity remain pending. See the
[foundation evidence](docs/validation/cluster_foundations_2026-09-07.md).

Media backend decision: FFmpeg alone handles demuxing, decoding, conversion
and encoding/muxing. VLC/libVLC and Qt Multimedia are not migrated. Thin audio
device adapters and the existing audio-feature analyzer remain separate; they
are not additional players. See `docs/media_pipeline_plan.md`.

Rhythm Master is an open-source project targeting commercial-grade reliability,
performance and usability. Payment, activation and proprietary editions are not
product assumptions. The project's exact outbound license has not yet been
selected; see `docs/third_party_reuse_policy.md` for dependency reuse rules.

The old repository remains a read-only source and behavioral reference for relevant
music visualization capabilities. Its full product scope is not a migration target.
Source is not copied without a dependency and ownership audit.

Documents:

- [Resonance Live walkthrough and validation](docs/validation/resonance_live_2026-09-08.md):
  music performance, exposed controls, reusable components and project/package workflow.
- [Live component viewers](docs/validation/component_previews_2026-09-08.md):
  instance-aware draft previews in the shared rendering runtime.
- [Product scope](docs/product_scope.md): music visualization workflows, scope boundaries
  and acceptance criteria; supersedes conflicting historical product assumptions.
- `docs/technology_stack_evaluation.md`: whole-product library evaluation,
  confirmed decisions versus candidates, boundaries and validation gates.
- `docs/gammaray_common_reuse_plan.md`: inspected first-party foundations,
  source/dependency findings and focused extraction/test sequence.
- `docs/cluster_playback_plan.md`: PC Host, QR admission, local rendering,
  synchronization, transport, venue capacity and staged acceptance.
- `docs/godot_3d_reference_plan.md`: confirmed primary 3D design/source
  reference, extraction boundaries and staged validation.
- `docs/media_pipeline_plan.md`: confirmed FFmpeg-only media backend, device
  boundary, shared playback clock, frame delivery and validation stages.
- `docs/architecture_overview.md`: target architecture, module boundaries and
  staged validation.
- [Phase A execution plan](docs/phase_a_execution_plan.md): multi-platform core
  validation, the first Windows workflow, minimal contracts, acceptance cases,
  dependency decision criteria and the actual unfinished implementation state.
- `docs/project_persistence.md`: current persistence audit and the proposed
  project/package format.
- `docs/commercial_product_plan.md`: product scope, distribution, security,
  commercial-grade quality, open-source delivery and release gates (historical
  filename retained).
- `docs/localization_plan.md`: language-neutral identities, catalogs, text
  rendering, IME behavior and cross-platform localization acceptance.
- `docs/coding_and_design_rules.md`: mandatory ownership, style, dependency and
  composition rules for project-owned code.
- `docs/visual_authoring_capability_plan.md`: capability research and the new
  typed Texture/Signal/Point/Scene/Material/time execution and editor model.
- `docs/builtin_nodes_and_presets_plan.md`: data-driven semantic nodes, presets,
  templates, catalog targets and easy-authoring acceptance gates.
- `docs/template_quality_delivery_plan.md`: latest minimum of 50 Basic and 50
  Advanced templates, the first representative batch and visual acceptance.
- `docs/third_party_reuse_policy.md`: open-source reuse, provenance, license
  compatibility and maintained-fork rules, including GPL code candidates.

The root `AGENTS.md`, `.clang-format` and `.editorconfig` enforce four-space
Google-style project code, explicit initialization, smart ownership and
composition-oriented design. Unavoidable third-party raw pointers are confined
to narrow adapters and never become project public APIs.

The implemented slice loads a data-defined graph, renders it through a private
bgfx/D3D11 adapter, and provides a docked node canvas, descriptor-based Inspector,
node creation/link editing, live parameter preview, undo/redo, asynchronous
transactional save/reopen, Chinese/English UI, and budgeted previews directly
inside image/3D and scalar/signal nodes. Up to eight visible nodes share a preview
budget, with selected-node priority and no routine CPU image readback. Images use
256x144 / 15 Hz capture; numeric nodes show evaluated values and up to 120 history
samples, including inside components. See [signal preview behavior and tests](docs/validation/signal_previews_2026-09-08.md).
The shared renderer also builds from its own directory without Studio dependencies.
Current Windows and Android checks include actual Adreno 650 GLES pixels and
playback of Windows-published runtime packages. Exact counts and remaining
capabilities are tracked in the implementation progress record below.
This does not establish Android App lifecycle acceptance.
See [runtime/Player validation](docs/validation/runtime_player_2026-09-07.md).
Current continuous implementation through Android is tracked in
[implementation progress](docs/implementation_progress.md).

On this Windows development machine (MSVC, CMake, Ninja, Python and the recorded
read-only SDK):

```powershell
./tools/prepare-dependencies.ps1
./tools/build.ps1 -Preset core
./tools/build.ps1 -Preset render-standalone
./tools/build.ps1 -Preset windows -ShaderCompiler 'C:/source/shark_dynamics_wallpaper/cmake-build-qt6/generated/bgfx_tools/bin/shaderc.exe'
python tools/build-windows.py
./tools/run-studio.ps1
```

The graphics build now compiles owned color-filter shaders for D3D11/GLES using
the validated host `shaderc` candidate. Set `RHYTHM_SHADERC` to your host compiler;
the example path is read-only and specific to this development machine. Android
accepts the same `-ShaderCompiler` argument on `tools/build-android.ps1`. Subsequent
builds preserve the configured path. Shader compilation/embedding is implemented
in Python; compiler/source/include hashes are retained beside generated artifacts.
Reproducible release distribution of the compiler remains a pending dependency
task; its executable is not bundled with Player.

For daily acceptance, `python tools/build-windows.py` builds **Release** with
20 workers and the validated local SDKs, and deploys both Studio and Player.
It preserves the separate `out/windows` Debug cache; use
`--configuration Debug` for debugging. The Studio launcher defaults to Release.
Do not use the Debug bundle to assess animation performance.

Studio's **导出音画 / Export A/V** toolbar action exports the applied work and
selected music to MP4 in a separate background host. Set the path, duration,
frame rate, size, codec and gain; completion displays a copyable file path.
Existing files are preserved. The initial Windows profile supports H.264 or
MPEG-4 with optional AAC, up to 1080p pixel count and one hour; arrangement and
range export remain separate work. See [export acceptance](docs/validation/studio_export_2026-09-08.md).

Use **作品音乐 / Work soundtrack → 保存当前音乐及设置 / Keep current music and settings**
to retain music with the work. Save/reopen restores the binding; publishing
includes it in one runtime package for Player. Music shares the existing 8 MiB
asset budget, so prefer compressed audio for longer tracks. See
[portable soundtrack acceptance](docs/validation/work_soundtrack_2026-09-08.md).

Every Windows application build automatically runs `tools/deploy-windows.py` to assemble
the sibling `deploy/` directory with the executable, recursively resolved
runtime DLLs, content, locales and third-party notices. Double-click
`out/windows-release/src/windows_spike/deploy/rhythm_master.exe` or use the launcher
above; no SDK PATH is required. Player is in
`out/windows-release/src/windows_player/deploy/rhythm_player.exe`.
Runtime DLLs are also copied beside the original build executable. CMake only
supplies build metadata and invokes Python; deployment logic lives in Python.
For a manual refresh without compilation:

```powershell
python tools/deploy-windows.py --config out/windows-release/src/windows_spike/deploy-config-Release.txt
```

For the large music-driven reference, search **共振星门 / Resonance Gate** in
the template browser, apply it, and click **播放演示音乐** in the audio panel.
The 164-node graph also responds to local music files or enabled system audio.
See [large-graph and audio evidence](docs/validation/resonance_gate_2026-09-07.md)
for frame times, the separate 1000-node interaction check and remaining limits.

The `windows_deploy_smoke` test starts the deployed app from an unrelated working
directory with a system-only PATH and checks that runtime DLLs load from `deploy`.
See the [deployment acceptance record](docs/validation/windows_deployment_2026-09-07.md).
Copy the entire `deploy` folder when moving the acceptance build. Default saves go to
the SDL per-user `RhythmMaster/Studio/Projects/Untitled.rhythmproj` directory.
Drag node headers, body text or empty body space with the left mouse button;
drag the colored circular sockets to connect nodes. Right-drag pans the canvas
with a hand cursor; the wheel zooms. Nodes have typed color accents, input sockets
on the left, output sockets on the right and matching link colors.
Select a node to edit properties, and use the toolbar to add
nodes, save/reopen, undo/redo or switch language. The output continues to use the
last valid compiled graph while an incomplete edit is diagnosed.

Shared Android development uses `python tools/build-android.py` for incremental
native builds and automatic Python APK packaging. The current local acceptance APK
is `out/android-arm64-release/apk/rhythm-player-release.apk`; install with `adb install -r`.
USB overlay installation and built-in effect selection are verified on the attached
Redmi K40S; broader lifecycle/endurance acceptance remains separately tracked.
SDK paths are script parameters, and target libraries are separate from host
protoc. The SDK is an audited experiment snapshot rather than a release lock;
see [dependency records](third_party/README.md).

The user authorizes continued development and broader implementation after
successful validation. The user subsequently accepted the Windows build and
authorized continuous implementation through Android completion. Remaining
Phase A and product gates are tracked alongside this work; pending dependencies
are validated before adoption, and a working demo does not close those gates.

Transparent windows/backbuffers are now tentative end-of-roadmap features by user
decision. Their current limitation does not block this slice or ordinary-window
backend adoption.

All currently discussed plans are filed here, under `docs/`; temporary drafts
outside this directory are not authoritative. The technology evaluation tracks
unresolved choices. Document completion does not imply implementation or test
completion. Start with that evaluation, then the architecture and relevant
feature plan; retain the Windows-before-mobile implementation gate.


The local Player now supports a bounded performance queue and 0–5 second SDR
scene dissolves. On Windows, open **Performance queue**; on Android, use
**Performance queue → Add built-in work**. Preparation keeps the active work
playing; pause freezes a transition, while seeking/restarting cancels it.
The queue lasts for the current performance. See [scene transitions](docs/scene_transition_plan.md)
for audio handoff, resource limits and focused device evidence.
