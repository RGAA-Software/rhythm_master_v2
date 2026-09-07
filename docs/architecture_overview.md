# Zero-Qt architecture and migration analysis

> Status: target architecture; first-stage validation and Windows slice authorized.
> Portable core and Windows Studio slice implemented; Phase A acceptance remains incomplete.
> Actual build/device evidence: [2026-09-06 validation](validation/phase_a_2026-09-06.md).
>
> Date: 2026-09-06

User priority update: transparent windows/backbuffers and related click-through
behavior are tentative features deferred to the end of the overall roadmap.
They do not block Phase A, current acceptance or ordinary-window backend adoption;
earlier phase lists mentioning transparent-topmost surfaces are superseded for
that feature. Existing platform boundaries remain applicable if it is implemented.

## 1. Product objective

User scope correction, 2026-09-07: [product_scope.md](product_scope.md) governs
this architecture. Desktop wallpaper hosting and desktop embedding are outside
scope and have been removed from session, platform and delivery requirements.

The 2026-09-08 target is a real-time node-based visual authoring product in the
class of TiXL and TouchDesigner, with music visualization as the core use case.
Studio must support original compositions from primitive nodes, inspectable
live results and reusable components; templates and Player serve this authoring
workflow. Audio, time and parameter inputs remain composable rather than making
every graph require an audio source. Its main
path is music file/live audio input, feature analysis, audio-driven visual graphs,
synchronized node/final previews, windowed/fullscreen playback, export and publishing.
Its primary workflow is closer to TouchDesigner, TiXL and a game-engine editor than to a
traditional form-based desktop application. The new application therefore uses
one GPU-oriented UI and rendering composition model instead of combining Qt
Widgets, QGraphicsView, QRhi, native transparent overlay windows and bgfx.

The final application must:

- have no Qt build or runtime dependency;
- provide the complete Studio and node-authoring workflow on Windows and macOS;
- provide the Player runtime on Windows, macOS, Android and iOS;
- include desktop-hosted cluster playback with QR-joined native Players as
  specified in `cluster_playback_plan.md`;
- support a freely dockable Dear ImGui workspace;
- display graph nodes, links, node viewers and final output in the same GPU
  surface;
- preserve the portable render, graph, effect, audio and physics capabilities
  already built in the existing repository;
- implement the typed visual-authoring model and content system defined in
  `visual_authoring_capability_plan.md` and
  `builtin_nodes_and_presets_plan.md`, rather than reproducing the old fixed
  component editor;
- finish and validate the Windows implementation before mobile implementation
  begins;
- support localized product UI and user-facing runtime metadata without using
  translated text as a persisted identifier;
- keep platform, UI, graph, runtime and graphics-backend types behind explicit
  boundaries.

Hardware protocol support and a general-purpose game framework are not initial
goals. Android and iOS influence portable runtime boundaries from the start,
but receive a Player host only after the Windows implementation is accepted.
They do not receive the Studio/node editor in the currently planned product.

Multi-platform design is required from the first shared module. Windows-first
orders host delivery and product acceptance; it does not defer portable
contracts, shared-library toolchain probes or dependency evaluation until a
later port. Track MSVC, macOS/iOS Apple Clang and Android NDK evidence separately.
By the user's 2026-09-07 decision, no Apple hardware is currently available:
delivery proceeds through Windows, Android Player, then the final Apple port.
Apple compilation, Metal and device validation are deferred to that final stage
and do not block Windows/Android progress. Shared boundaries and compatibility
review still cover Apple now; its matrix entries remain explicitly unvalidated.
Cross-compiling shared libraries and running portable conformance tests do not
authorize or require early mobile application-host development.

## 2. Existing-project audit

The existing repository contains roughly 1,200 project C++ source/header files.
Approximately 436 directly reference Qt. Those references are concentrated in
`src/ui`, `src/maker`, `src/editor`, the application shell and Qt render-surface
hosting. Rewriting the visible application does not require discarding all of
the current engine work.

### 2.1 Candidate modules to migrate after audit

- `Rhythm::Render` and its null/bgfx backends;
- `Rhythm::Graph` document, validation, commands and compiler;
- Effect Graph Schema and Effect Graph IO;
- Effect Player Runtime;
- audio feature extraction and spectrum processing;
- Box2D-isolated physics and project-owned particle simulation;
- lighting, post-processing, shader, texture and 2D render primitives;
- Protobuf schemas and data migration tests;
- renderer-null, screenshot and deterministic scene tests.

Each candidate must compile in the new repository without Qt before it is
accepted. Existing Qt-facing loaders or host adapters are not part of the
portable module merely because they live in the same old source directory.
Migration is not the product design: accepted code must implement the new typed
operator, execution-plan and package contracts. Fixed-effect runtime branches
or old component schemas do not become new public APIs merely to reduce porting
work.

### 2.2 Modules to replace

- QMainWindow/QWidget application and Maker windows;
- QWindowKit and custom Qt title bars;
- QtNodes/QGraphicsView and Qt graph painters;
- QWidget Cyber controls and Qt property inspectors;
- QRhi and native transparent node-preview overlays;
- Qt model/view gallery and audio lists;
- Qt render-surface host and event-loop integration;
- Qt file/path/process/IPC/tray wrappers;
- all VLC/libVLC and Qt Multimedia playback paths, replaced by the FFmpeg-only
  media pipeline; audited native capture code can remain in device adapters.

This is a layer replacement, not a widget-by-widget translation. Copying the
old QWidget composition into immediate-mode functions would preserve the old
architecture and its coupling.

### 2.3 GammaRay foundations

The user authorizes reuse of our own code in
`D:/GoCloud/GammaRayPremium/src/px_deps/px_common`. Prefer focused async, I/O,
reconnect, diagnostic and QR extraction, not the common aggregate target or
remote-desktop video protocols. Embedded third-party sources retain their
licenses; platform support and security defaults need validation. See
`gammaray_common_reuse_plan.md` for actual inspection findings.

## 3. Stack direction and confirmed decisions

`technology_stack_evaluation.md` is the whole-product selection register. The
table below is a concise direction, not a claim that every library is frozen,
integrated or validated. FFmpeg-only media and Godot-primary 3D reference are
user-confirmed; concrete versions and pending device/network choices remain
explicit validation tasks.

| Responsibility | Selected direction |
| --- | --- |
| Window, input, DPI and lifecycle | SDL3 |
| UI composition | Dear ImGui docking |
| Graph canvas | project-maintained imgui-node-editor fork |
| Render abstraction | RhythmRender |
| Private GPU backend | bgfx |
| Physics | project wrapper around Box2D |
| 3D scene/material/model/animation design | Godot as primary design and source reference; focused adaptation, not full-engine embedding |
| Media demux/decode/encode/mux | FFmpeg only; see `media_pipeline_plan.md` |
| Audio analysis | audited existing portable audio-feature implementation |
| Audio device I/O | thin platform/device adapter, not another player; implementation choice pending |
| Authoring/runtime schemas | Protobuf plus small JSON manifests |
| Operator/content definition | focused typed registries plus versioned data packages |
| Filesystem paths | `std::filesystem::path` |
| Text at UI boundaries | UTF-8 strings |
| Background work | focused task/cancellation contracts; adapt GammaRay async with private Asio and bounded workers |
| Cluster transport | reliable control + timely datagrams; QUIC preferred pending library spike, WSS compatibility path |
| Desktop native file dialogs | Native File Dialog Extended or equivalent adapter |

SDL3 is a platform backend, not the renderer. Dear ImGui creates draw data;
RhythmRender consumes translated UI draw lists; bgfx remains private to the
render implementation.

FFmpeg is the confirmed media backend, not merely one of several alternatives.
Use its libraries directly behind focused adapters, not an embedded ffplay
window or an ffmpeg subprocess for normal playback. Platform device I/O and
hardware-frame interop remain explicit boundaries. A single project-owned
playback/session model controls timing, seek and looping; existing spectrum
analysis consumes normalized PCM without another decode/filter pipeline.

## 4. Dependency direction

```text
apps/rhythm_master
        |
        +--> studio_ui ---------> editor_application
        |       |                         |
        |       +--> graph_editor_adapter +--> graph_domain
        |       +--> ui_render_bridge ----+--> rhythm_render
        |
        +--> platform_api <--------- platform_sdl / platform_windows
        +--> project_service -------> project_format / asset_store
        +--> player_service --------> effect_runtime

effect_runtime --> graph_runtime --> rhythm_render
audio_runtime ---------------------> effect_runtime
physics wrapper -------------------> effect_runtime / rhythm_render

rhythm_render implementation ------> bgfx
```

Forbidden dependencies:

- graph and effect domain code must not include SDL, Dear ImGui or platform
  headers;
- public RhythmRender headers must not expose bgfx types;
- UI panels must not issue platform-native calls directly;
- platform APIs must not know graph node types;
- persisted projects must not contain pointers, GPU handles, native window
  handles, Box2D objects or ImGui IDs.

Cluster services publish immutable typed inputs into effect runtime. Transport,
room admission, clock mapping and asset distribution are outside the standalone
render library. Graph/runtime public headers do not expose Asio/cpr/QUIC types.

## 5. UI and rendering composition

The main studio window owns one native SDL window and one RhythmRender surface.
The same surface draws:

- the application navigation and dock space;
- project/library panels;
- graph nodes, ports and links;
- Inspector and diagnostics;
- node viewer textures;
- final-output viewer textures;
- settings, player controls, modal dialogs and notifications.

Effect evaluation renders to RhythmRender targets. Viewers sample the existing
`TextureHandle`; they do not read pixels back to the CPU and do not create a
native overlay window.

```text
Effect graph -> render target -> RhythmRender TextureHandle
                                      |
                                      +-> node viewer
                                      +-> final viewer
                                      +-> cover capture
                                      +-> preview/fullscreen surface
```

Dear ImGui must not receive a public `bgfx::TextureHandle`. A UI texture
registry maps stable UI texture IDs to project `TextureHandle` values. An
ImGui draw-data translator uploads vertices/indices and emits scissored UI draw
commands through RhythmRender.

The process owns one renderer device and one frame boundary. Independent music
visualization preview and fullscreen playback windows are additional surfaces
sharing that device and frame boundary. Tentative native transparency follows
the separate deferred feature decision; it does not imply desktop embedding.

### Offline A/V export implementation (2026-09-08)

Studio captures an applied project snapshot and submits it to `ExportJobs`.
A bounded background executor stages assets and music, then the private SDL
process adapter launches the deployed executable's hidden `--export-job` host.
That process owns its renderer and evaluates the shared Runtime at exact frame
times. Three GPU readback tickets feed a separate encoder thread with two queued
frames and one active frame. This also keeps FFmpeg Media Foundation's thread
apartment separate from SDL window hosting. Interactive viewers remain GPU-only.

The controller requires a success receipt and complete frame count before
atomically publishing a new MP4 without replacing an existing destination.
Cancellation reaps the child before owned staging cleanup. Project contracts
carry values, paths and handles; SDL/FFmpeg types remain private adapters.
This is a local offline job, independent of the deferred cluster transport.
See [implementation and validation](validation/studio_export_2026-09-08.md).

### Authored soundtrack implementation (2026-09-08)

Soundtrack identity, gain and repeat settings are project values alongside the
graph snapshot. A bounded authoring worker imports content-addressed music and
probes it through the existing media adapter. The UI validates the document,
revision and current music selection before applying the returned snapshot.
Package preparation publishes compressed music bytes or a retained file range
whose lifetime is shared by Session and the audio decoder worker. Host adapters select that
source and feed the same playback time/features into the graph; Session does
not become a second decoder or audio-device owner.

Small music-bearing packages use `music-performance-v1`. Larger songs use
`music-performance-v2`: one stored music attachment up to 256 MiB, ordinary assets
up to 8 MiB, at most 64 combined records and a 272 MiB file archive. Both use
program ABI 2. ZIP metadata, hashing and file copies are bounded; FFmpeg seeks
inside the retained range without buffering the whole song. The audio worker explicitly acknowledges source
replacement before host-owned imported files are reclaimed. See
[workflow and platform evidence](validation/work_soundtrack_2026-09-08.md).
The [large-song evidence](validation/large_music_packages_2026-09-08.md) includes
actual Studio authoring, Windows playback and USB Android PCM/GLES. APK lifecycle
acceptance remains pending; native tests do not establish it.

## 6. Application and session model

One executable supports explicit launch modes:

```text
rhythm_master studio
rhythm_master player
rhythm_master preview <project>
rhythm_master render-test <project>
rhythm_master cluster-host <published-package>
```

The application composes focused sessions such as `StudioSession`,
`PlayerSession` and `CaptureSession`. A single executable
does not require every task to share one OS process forever. Process isolation
may remain an operational policy, while code, commands, persistence and runtime
stay unified.

Studio is a desktop product. Windows and macOS share the graph domain,
compilation pipeline, inspector descriptors and ImGui workspace, while their
windowing, menu, signing and platform integration remain separate adapters.

The mobile product is a Player host, not a reduced copy of Studio. Android and
iOS consume validated published runtime packages and expose playback controls,
surface lifecycle, audio input/playback and product settings. They do not link
editor panels, graph mutation commands, compiler UI or desktop file dialogs.

Cluster Host composes room, clock, input replication and asset services; Studio
only adds a control panel. Participants run Player, with mobile QR/deep-link
entry rather than a desktop command line. Windows hosting is first; macOS
hosting arrives in the final Apple port. Phones are participants, not editor or host implementations.

## 7. Platform boundary

Platform responsibilities are represented by focused interfaces, not a single
global platform class:

- application/event lifecycle;
- windows, displays, DPI and native handles;
- file dialogs and shell/open-URL integration;
- paths and filesystem operations;
- clipboard, cursor, drag/drop and text input;
- process launch and single-instance IPC;
- desktop tray and notifications;
- audio device capture;
- power/session events.

SDL calls remain in the SDL implementation. Win32/Cocoa/UIKit/Android native
types remain in their narrow platform adapters. Custom title-bar hit testing,
DWM shadow/snap support are Windows adapter responsibilities. Native transparency
and click-through remain tentative end-of-roadmap capabilities in that adapter;
desktop embedding is outside the product scope.

### 7.1 Supported product matrix

| Capability | Windows | macOS | Android | iOS |
| --- | --- | --- | --- | --- |
| Studio/node editor | Yes | Yes | No | No |
| Published-package Player | Yes | Yes | Yes | Yes |
| Cluster participant | Planned | Planned | Planned | Planned |
| Cluster Host | Planned first | Planned later | No initially | No initially |
| Project authoring/import | Yes | Yes | No | No |
| Offline render/export | Yes | Yes | No initially | No initially |
| Windowed/fullscreen preview | Yes | Yes | Fullscreen/app surface | Fullscreen/app surface |

All four Player implementations use the same portable graph runtime, effect
runtime, package validation, asset model and RhythmRender public API. Platform
hosts own only lifecycle, surfaces, input, audio devices, safe storage and
store/platform services. Published packages declare required runtime features;
unsupported packages fail validation with a localized diagnostic rather than
partially executing.

## 8. Text, localization, input and DPI

The first Windows vertical slice must validate Chinese input before broad UI
migration. It must cover composition events, candidate-window placement,
selection, clipboard, undo/redo, shortcuts and focus transfer. Fonts use
FreeType-backed atlases and UTF-8 application text; complex-script shaping can
be isolated behind a text service if required.

All source text and cross-module text values use UTF-8. User-visible strings
use stable message IDs and locale catalogs; English text is never used as an
identity key. Node type IDs, property keys, command IDs, Protobuf fields and
serialized enum values remain language-neutral. Node titles, property labels,
diagnostics and help text are resolved only at presentation time.

The localization service provides BCP 47 locale selection, operating-system
locale detection, explicit user override, deterministic fallback, parameter
substitution, plural forms and locale-aware number/date formatting. Catalogs
are packaged separately from code so translation changes do not rebuild the
render or graph runtime. The initial supported locales are Simplified Chinese
and English; adding a locale must not require changes to persisted projects.

Font fallback and glyph coverage are selected per locale. Complex shaping and
bidirectional text are handled behind the text service rather than assumed to
work through basic Dear ImGui text calls. ImGui widget identity uses stable
hidden IDs, so changing language cannot reset dock layout, focus or settings.
Published packages may contain localized display name, description and author
metadata with an explicit fallback locale, but runtime behavior never depends
on those translations.

Logical UI units and render pixels are distinct. SDL display-scale events feed
one DPI policy. Graph coordinates and effect design coordinates never use raw
desktop pixels.

## 9. Execution and threading

Main thread phases:

1. poll platform events;
2. apply commands and completed background work;
3. build the current ImGui frame;
4. determine graph viewer demand roots;
5. update effect state using bounded frame/fixed-step timing;
6. submit offscreen effects, node viewers and UI draw lists;
7. present all due surfaces once.

Graph compilation, asset import, shader compilation, database and network work
run outside the main thread and publish immutable/value results. Rendering and
UI callbacks must not block on those jobs.

Network I/O is event-driven with bounded queues, not one blocking thread per
phone. Reliable commands, latest-value features and resource downloads have
different scheduling policies. Audio callbacks do not decode, log or perform
network I/O. Session-time mapping feeds existing runtime clock contracts.

## 10. Migration strategy

The new target must be zero-Qt from its first build. The old application stays
available only as a behavior/data reference until replacement.

### Phase A: risk-first Windows vertical slice

Execute [the Phase A plan](phase_a_execution_plan.md): P0–P4 cover the
multi-platform core, build/data contracts and dependency evidence; W0–W5 cover
the first Windows host and its risk tests. The minimal workflow ends at W4;
W5 closes the Windows risk tests and reports the shared validation matrix.
Windows results alone cannot close multi-platform architecture validation.

1. SDL3 high-DPI resizable window and platform event loop.
2. RhythmRender/bgfx surface and Dear ImGui renderer bridge.
3. Cyber theme, Chinese text input and custom title bar.
4. imgui-node-editor with real graph-domain commands and persistence.
5. one real effect graph with final output and multiple node viewers.
6. 4K interaction, multi-monitor DPI and device-loss diagnostics.

### Phase B: authoring capability

1. typed operator domains, explicit bridges and demand-driven execution plans;
2. Inspector generated from node/property descriptors;
3. resource browser, import queue and content-addressed assets;
4. graph diagnostics, history, semantic components and viewer controls;
5. constant/binding/expression/keyframe parameter modes and component time;
6. versioned semantic-node, preset and template package registry;
7. project save/recovery/export and current-project importer;
8. profiler for UI, graph evaluation, render passes and GPU memory.

### Phase C: music visualization product workflows

1. installed/made project library;
2. music playback/capture, audio-driven visuals and synchronized animation transport;
3. settings, themes and localization;
4. node/final previews, windowed/fullscreen music visualization playback and audio-synchronized export;
5. single-instance command routing, tray, Steam and network services.
6. desktop cluster Host/Player and simulated-peer tests (cluster C1-C3);
   these do not establish mobile or venue-capacity acceptance.

### Phase D: Windows cutover

1. run relevant music visualization projects and compare audio response, synchronized playback and visual output;
2. switch Windows distribution to the new executable;
3. remove Qt, QtNodes, QWindowKit and obsolete UI build targets;
4. stop for Windows acceptance before implementing mobile frontends.

### Phase E: Android Player host

1. implement Android lifecycle, surface and safe-storage adapters;
2. integrate touch input, audio sessions and interruption/background policy;
3. validate the shared runtime against mobile GPU and memory budgets;
4. implement signed package acquisition and mobile release packaging;
5. do not migrate Studio, node editing or desktop-only services.
6. native QR cluster join, permissions and resynchronization; run cluster C4-C5
   on actual mobile devices and venue networks before publishing capacity.

### Phase F: final Apple platform port

1. obtain Apple toolchains/hardware and validate shared targets on macOS and iOS;
2. implement macOS SDL/native adapters, Studio and Player integration;
3. implement the iOS Player lifecycle, touch, audio, storage and cluster adapters;
4. validate Metal, Retina, IME, menus, permissions and background behavior per platform;
5. verify project/package parity and device GPU/memory budgets;
6. sign, notarize/package macOS apps and complete iOS device/store acceptance.

This ordering supersedes the previous macOS-before-mobile sequence by the user's
2026-09-07 decision. Apple remains a target throughout shared architecture work.

## 11. Acceptance gates and milestone mapping

These requirements span the initial slice and the complete Windows capability
validation. The first slice must demonstrate Value/Signal/Texture, generic
content loading, editing, shared GPU viewers and atomic save/reopen. The
Texture/Signal/Point/Scene composition below remains mandatory for the later
authoring/Windows capability gate, before Windows cutover. Its placement here
does not require implementing all those domains before the first window test.
The [execution plan](phase_a_execution_plan.md) maps concrete cases to steps;
the capability and product plans retain their broader completion requirements.

- shared graph/runtime/render and codec modules have per-platform build/test
  evidence, platform-free public contracts and separate Studio/Player targets;
  missing evidence remains pending, not inferred from Windows success;

- the new executable and all transitive targets contain no Qt dependency;
- a 4K graph remains responsive with a representative large graph;
- final output remains at target frame rate while several visible viewers are
  budgeted independently;
- node viewers share GPU resources and perform no routine CPU readback;
- Chinese text input, file drop, clipboard and shortcuts work correctly;
- switching between Simplified Chinese and English does not change graph,
  project or widget identity and requires no restart unless a font atlas must
  be rebuilt;
- moving between 4K and lower-DPI displays preserves logical UI size;
- save interruption cannot corrupt the last committed project revision;
- the Null backend and deterministic graph/runtime tests remain headless.
- a preset/template is loaded through the generic content registry and never
  through a bespoke demo-scene branch in application code;
- a representative graph combines Texture, Signal, Point and Scene domains,
  including explicit feedback and intermediate viewers, without using a fixed
  legacy effect execution path.

Platform completion is tracked independently: Windows acceptance does not
claim macOS Studio or mobile Player completion. Player conformance tests run
the same published packages and deterministic graph cases on every supported
platform.

Numeric performance budgets must be based on a recorded reference machine
rather than unsupported universal FPS claims. Frame telemetry must separately
report UI build, graph evaluation, render submission, GPU execution and present
latency.
