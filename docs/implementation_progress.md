# Continuous implementation through Android Player

User direction, 2026-09-07: continue implementation without stopping at each
module delivery, through completion and validation of Android Player. The USB
device is available for testing. Apple is the final platform stage.

The accepted Windows build remains the integration baseline. Work proceeds
incrementally through the existing capability, media, content, persistence and
cluster plans; an installed demonstration APK alone is not Android completion.
Pending dependencies are validated per module before adoption. Outstanding
feature and platform checks remain explicit, including Windows risk checks.

## 2026-09-08: music authoring scope, performance and common playback time

The user confirms TiXL/TouchDesigner-class node-based visual authoring as the
product target, with music visualization as its core use case. Wallpaper hosting
is excluded. The latest instruction authorizes continuing all five work streams
through Android acceptance: complex-graph performance, parameter/time control,
reusable components, complete authoring-to-playback examples and quality content.
Communication remains paused and Apple is the final platform stage.

Dynamic intermediate texture lifetimes, optional node CPU profiling and paused
frame reuse are implemented and tested on Windows and Android. Resonance Gate
reduces texture memory substantially; stable 60 fps is not established on Android.
See `validation/texture_lifetimes_2026-09-08.md`.

Music and graph time now share the actual audio-consumption estimate in Studio
and Player, including pause, seek, loop and temporal-history reset. Windows
audio/graph integration and Android native contracts pass; see
`validation/music_transport_2026-09-08.md`. The Android music APK now includes
vcpkg FFmpeg and local music controls with a tested companion relink bundle;
see `validation/android_music_application_2026-09-08.md`. Phone installation,
app lifecycle/audio and long-run acceptance remain distinct pending checks.
The five-stream program and 50 Basic + 50 Advanced content target are unfinished.

The cross-project user component library now saves nested definitions, exposed
controls, layout and referenced assets, and inserts them with collision isolation
and undo. English/Chinese ImGui interaction tests and Windows/Android core
round-trips pass. See `validation/user_component_library_2026-09-08.md`.

Resonance Live adds a structured music performance with two editable components,
197 reachable instructions, PBR rings and an opening curve. Real decoded PCM
changes its D3D11 output; the complete authoring API workflow passes on Windows
and the USB Android device. That workflow also found and fixed nondeterministic
multi-property component hashes. Native mobile timing remains below stable
60 fps even at economy quality. See `validation/resonance_live_2026-09-08.md`.
Inventory is now 30 examples; media arrangement,
50 + 50 content quality and Android application acceptance remain unfinished.
Windows Studio now exports the applied work and selected music to MP4 through
an independent host with bounded rendering/encoding queues. See
`validation/studio_export_2026-09-08.md` for its supported profile and limits.

Component workbench viewers now reuse the root graph runtime with concrete
instance mapping, transient draft preview, shared eight-image budgeting and
generation-bound cache invalidation. Actual Studio/D3D11 and ImGui regressions
pass; see `validation/component_previews_2026-09-08.md`.

Selected instances can now be unpacked one level with parameter, signal,
connection, layout and undo preservation. Four real decoded-PCM D3D11 images
remain pixel-identical after unpacking the performance core. The portable command
also passes on Android; see `validation/component_unpack_2026-09-08.md`.

## 2026-09-07: complex music-driven scene and large-canvas verification

Resonance Gate adds a fully connected 164-node / 269-edge Advanced audio template,
24 radial band controls, actual PCM demo playback and a reusable eight-input
texture stack. The stack fixes the intermediate-texture budget failure exposed
by the first 183-node composition. Actual decoded music/silence/low/high captures
differ; the Release editor measures approximately 60 fps at 720p output during
zoom/pan with live file analysis. A separate 1000-node canvas drag/pan regression
passes. See `validation/resonance_gate_2026-09-07.md` for exact scope and remaining
limits. Inventory is now 29 examples, 11 semantic components and 128 preset
records; quality acceptance and the broader five-item program remain incomplete.

## 2026-09-07: editor performance and optimized acceptance builds

Firefly Garden's CPU work was dominated by the unoptimized Debug build. The
same full editor with eight previews changes from 32.3 ms to 3.0 ms median
Studio graph/UI construction in Release, reaching approximately 60 fps in the
recorded 1080p host test. Python Windows builds and the launcher now default to
the separate optimized acceptance bundle. Debug caches are preserved. See
`validation/editor_performance_2026-09-07.md` for controlled measurements,
deployment paths, checks and scope limits.

## 2026-09-07: shrinking popup corrected

The user-reported template-browser shrink is reproduced as a one-pixel-per-frame
autosizing feedback loop. Four browsing/asset popups now have stable viewport-
bounded sizes. Actual 300-frame UI checks cover live preview, empty search and
reopening. See `validation/popup_sizing_2026-09-07.md`.

## 2026-09-07: five-item continuation, still in progress

Signed image displacement, RGBA16F temporal trails and deterministic curl-field
particles now reach graph, inspector and Player. The catalog has tier/subject
filters, search, actual rendered thumbnails and a selected live preview. Three
new references complete the first authored 3 Basic + 3 Advanced candidate batch;
the new torus primitive reuses Godot sampling. The three corresponding semantic
components bring inventory to 11 components, 126 preset records and 28 examples.
These counts do not establish final visual acceptance or 50 + 50 completion.

The USB Android device reconnected. Selected native CPU/GLES contracts pass,
including signed displacement, RGBA16F decay and device recreation. Release
offscreen measurements cover all six references at 720p with stable texture
allocation; Prismatic lotus still needs a reduced mobile render extent. The
earlier installation restriction still prevents claiming actual APK acceptance.
See `validation/reference_batch_2026-09-07.md` and
`validation/displacement_trails_2026-09-07.md` for exact evidence and limitations.

Windows media integration now uses a validated vcpkg-only LGPL profile, without
selecting the project's outbound license or upgrading the shared SDK. Studio and
Player share local music controls, canonical analysis, pause/seek/volume/repeat
and suspension behavior. Each complete deploy contains 21 DLLs and the matching
FFmpeg source/build materials. Video decoding validates RGBA/PNG alpha,
PTS/VFR/B-frame drain, rotation, seek and embedded memory ownership. Static image graph assets now pass import/preparation, save/reopen, publish and
Player/device-recreation checks, including real D3D11 alpha/framing pixels.
Video graph consumers now support bounded embedded clips, independent node
playback, fit/fill, loops and seeks with stable GPU uploads. Shared authoring/media
transport remains unfinished; see the later media validation section for limits.
The isolated API-26 Android profile passes PNG and embedded decoding on the USB
device; application audio and actual APK acceptance remain outstanding. See `validation/media_application_2026-09-07.md`.

Android Release measurements now also cover the shared balanced quality policy.
At 960x540 all six references have p95 below 30 ms in the short native offscreen
test; actual display and long-duration thermal acceptance remain outstanding.
The APK offers persistent original/balanced/economy quality. Its Python packager
now uses the actual build directory/configuration, so Release native binaries
cannot silently be replaced by the old Debug build during APK assembly.

## 2026-09-07: template title encoding corrected

The prismatic-lotus label was incorrectly GBK-decoded in authored metadata; changing
fonts did not repair it. The manifest/generator and saved review-project title are
corrected, and package/GPU checks now assert the actual expected text. Microsoft
YaHei is restored at the user's request. Both applications and matching review
packages are redeployed. See `validation/template_title_encoding_2026-09-07.md`.

## 2026-09-07: complex visual reference

Prismatic lotus adds an editable Advanced reference using two counter-rotating
procedural fields, polar depth, mirrored sectors, antialiased luminance contours,
soft aperture, star dust and two glow scales. Actual Player motion captures were
inspected and iterated for readable highlights and lower central noise. New
mapping/contour nodes have categorized palette entries, localized controls and
six presets. Render-thread shader resources now have a focused lifetime owner.
See `validation/prismatic_lotus_2026-09-07.md` for build, pixel and artifact evidence.
This is not completion of 50 Basic / 50 Advanced or the seven-step product scope.

## Product acceptance correction

The user's latest review rejects the unclassified node list and the visual
quality of the current templates. The product is not complete. Previous Windows
acceptance covered an earlier interaction/deployment slice, not final authoring
usability or the content library. Passing contract, pixel and playback tests
establishes specific behavior; it does not establish artistic quality.

Catalog inventory is eight semantic components, 111 preset records and 25 runnable
example projects. These are not counts of visually accepted deliverables toward
the content product targets. In particular, the 25 examples must not be reported
as 25 polished complete templates. Final visual acceptance remains outstanding.
See `validation/authoring_quality_correction_2026-09-07.md` for the immediate fix
and the remaining acceptance requirements.

Latest user target: at least 50 Basic templates and 50 Advanced templates,
counted separately, superseding the former 24-template minimum. The 40 semantic
nodes and 120 presets remain required. Next work prioritizes reusable visual
effects and a representative polished content batch before volume expansion;
see `template_quality_delivery_plan.md`.

## Execution priority correction

Source-reuse correction, 2026-09-07: the user requires direct use, extraction or
adaptation of suitable mature open-source implementations before writing equivalent
functionality ourselves. This applies throughout steps 1–7. Inspect the relevant
reference first, keep necessary project adapters focused, and record concrete gaps
when a new implementation is needed. Follow `third_party_reuse_policy.md` for
provenance and actual-license handling; prefer vcpkg for dependencies. This rule
does not change the paused communication work or the Apple-last platform order.

Dependency sourcing correction, 2026-09-07: prefer `C:/source/vcpkg` for all
third-party libraries and build tools. The custom FFmpeg 8.1.2 build processes
were stopped and the project-owned builder was removed. Existing build/install
artifacts under out/media-sdk are unused and must not be adopted.
Use installed Windows 6.1.1 / Android 6.1 packages for media validation.
Existing source/fork gaps are recorded in `validation/vcpkg_dependencies_2026-09-07.md`;
this correction does not mean all existing dependencies have been migrated.

The user questioned why communication was being developed before the main
features were complete. The core product is still incomplete. Initial transport
validation was justified by the pending dependency and Android requirements,
but expanding it into room-service integration before the authoring/audio/content
work was complete displaced the product's main path.

Superseding user instruction: STOP communication development and testing now,
including the unfinished drain-close regression. Preserve its current code and
evidence without adoption. Communication is the final overall stage, after Apple.

The user explicitly authorized continuous implementation of these seven steps.
The 2026-09-07 product-scope correction removes wallpaper hosting from step 6;
[product_scope.md](product_scope.md) defines the music visualization acceptance path:

1. Local music-driven visuals: canonical audio analysis, music playback/capture,
   frequency/loudness/onset inputs and visual parameter connections.
2. Common visual tools: image/video sources, transforms, masks, compositing and filters.
3. Authoring workflow: parameter binding, safe expressions, timeline and reusable components.
4. Advanced visual domains: particles, physics, 3D scenes, materials and lighting.
5. Content library: at least 40 semantic nodes, 120 presets, 50 Basic templates
   and 50 Advanced templates under the latest user target.
6. Windows music visualization product closure: project library, settings,
   synchronized audio/visual controls, windowed/fullscreen playback, export/publishing,
   performance and stability acceptance.
7. Android local Player: shared capabilities, lifecycle, touch, audio, imports,
   orientation, GPU/memory budgets and actual APK/device acceptance.

Each increment builds and validates its affected module before further expansion,
then reaches the application so acceptance is based on usable behavior. The
existing FFmpeg license-choice and USB APK-install prerequisites remain pending;
they do not justify diverting independent work into communication again.

Existing networking results are retained as candidate-module evidence. They do
not imply completion of the main product or take priority merely because their
next implementation steps are well defined.

## Implemented increments

Visual effects increment: focused TiXL shader reuse adds Gaussian blur and spatial
fractal noise, including graph nodes, inspector presets and bounded GPU resources.
The first two Basic template candidates are Layered neon audio orbit and Aurora
clouds. They are data-authored graphs with editable components; visual acceptance
is still pending. Inventory is now 24 runnable examples and 105 preset records
(89 operator + 16 semantic presets), not 24 accepted polished templates. The
50 Basic / 50 Advanced targets remain open. A surface-resize/cache defect exposed
by actual preview capture was also fixed. See
`validation/visual_effects_2026-09-07.md` for source records and exact evidence.

Semantic library increment: the Studio effect-component palette loads eight
editable semantic nodes, reusing existing graph/component/codec services and
template data. Insertion preserves the current graph and is one undoable edit.
Each component has Default and a curated variant; incompatible presets preserve
current values. All eight initial default packages pass actual Windows playback.
Current content: eight semantic nodes, 99 presets (83 operator + 16 component),
22 runnable example projects; visual acceptance and the latest catalog targets
remain outstanding. See
`validation/semantic_library_2026-09-07.md` for source review, tests and limits.

Static GLB integration: typed asset references, bounded background model preparation,
Studio selection/previews, publish-time model validation and Player lifecycle now
connect the cgltf importer to applications. Real Cesium Box passes Windows package,
rollback/recreation and GPU tests; 25 scene pixel cases pass. Studio/Player smoke
tests run the standalone GLB package and show inline previews. Android shared code,
tests and APK cross-build, but the disconnected device still prevents new actual
acceptance. Catalog: 83 presets / 22 templates. Details, provenance, limits and paths:
`validation/glb_integration_2026-09-07.md`.

Earlier 3D increment: Godot projection, UV sphere and focused GGX/Schlick BRDF source,
private vcpkg GLM mathematics and bounded cgltf static GLB parsing are in place.
Ten procedural scene/material/geometry/camera/light operators now reach Studio,
inline previews, publishing and Player. Windows passes 38 selected integration
suites and 22 real scene GPU cases. Five new templates complete actual Windows
Player playback; content totals 82 presets / 22 templates. Android shared targets,
GLES shaders and APK cross-build; the device is currently absent from ADB, so the
new GPU cases have not run there. The later GLB integration above adds asset binding
and background preparation. See `validation/scene_nodes_2026-09-07.md` for exact scope, reuse
and limits. This does not complete all of step 4 or step 7.

Installed-vcpkg media adapters now decode local WAV/FLAC into bounded canonical
PCM on Windows and USB Android. A thin SDL output adapter and asynchronous file
playback/analysis coordinator pass actual Windows device tests, including pause,
seek generations, EOF, cancellation and recovery. Android builds the same output
and playback code; actual audio requires APK/Java-host acceptance. The FFmpeg
path remains isolated pending the application's license decision. See
`validation/local_media_2026-09-07.md` for scope and remaining work; this is not
completion of step 1 or Android step 7.

Local audio now reaches the applications: Windows Studio and standalone Player
offer explicit system-loopback input; the Studio inspector shows live levels and
63 bands. Two audio scalar operators and a Music Gradient template use the same
compiled graph/package path. The first audio increment passes 39 Windows suites,
31 Android native suites and explicit Windows device open/restart/stop checks.
See `validation/local_audio_2026-09-07.md` for provenance, exact scope and remaining
music-file playback / mobile audio work. The subsequent spectrum increment adds
linear/radial batched geometry, transparent compositing and a Neon Spectrum Ring
template. It passes 40 Windows suites and 32 Android native suites, including
actual GLES pixel checks for both layouts and silence reset. There are now
24 presets and 5 templates. The following 2D-transform increment adds
`texture.affine` with scalar-connected scale/rotation/translation/opacity,
mirroring and pivots; rotating-card and music-pulse-card templates bring the
library to 27 presets and 7 templates. Windows passes 41 suites; Android passes
33 native suites, including exact affine output pixels. Internal premultiplied
GPU storage fixes translucent coverage across repeated passes; upload, vertex
tint and clear-color paths are checked on GLES. Legacy transform nodes/packages
retain their original schema and input slots.

Shapes, normal/inverse alpha masks, source-over/additive layer compositing and
safe arithmetic expressions now reach Studio, portable runtime and publishing.
Windows passes 43 suites and USB Android 35 native suites, with exact GLES
compositing pixels and bounded expression parsing/evaluation. There are now
37 presets and 10 templates. See `validation/local_authoring_2026-09-07.md`.
The initial Timeline panel adds transport, seconds/frames/beats display,
stateless seek/loop and undoable curve-track editing. Pausing freezes feedback
and captured external inputs; feedback seek remains disabled until replay exists.
Windows now passes 44 suites, Android 35 native suites. Full track editing,
named bindings and reusable components are still outstanding.
Color adjustment now runs as an owned D3D11/GLES shader with scalar-controlled
exposure, contrast, saturation and inversion. Windows passes 45 suites, Android
36 native suites, including filter pixels; both GPU hosts play the published
Breathing Light template. The content library now has 41 presets and 11 templates.
The development shaderc candidate is used read-only; reproducible release-tool
provenance remains pending, as recorded in the authoring validation document.

Graph-scope named signals and input bindings now reach the Inspector, canvas,
history, template instantiation, authoring persistence and compiled publishing.
Windows passes 46 suites; Android passes 37 native suites. The Shared Pulse
template binds one signal to scale and gradient color, bringing the library to
41 presets and 12 templates. Its published package passes actual Windows and
Android GPU playback. Authoring schema 3 stores bindings; schema 1/2 reading and
compiled ABI 2 remain supported. Nested components are still outstanding.
See `validation/named_bindings_2026-09-07.md`.

Embedded nested components now compile to primitive instructions, retain stable
instance viewer IDs and expose grouped, bounded public controls. Studio supports
selection wrapping, repeated instances and undoable project expansion. Authoring
schema 4 retains the library; compiled ABI 2 stays unchanged. Windows passes 49
suites and USB Android 40 native suites; the Pulse Card Component template brings
the library to 41 presets / 13 templates. See `validation/components_2026-09-07.md`
for exact scope and remaining component editing work.

Component drafts now support nested canvas navigation, public interface editing,
shared undo, revision conflict checks and independent deep copies of a selected
instance. Internal layouts persist as editor schema 2 metadata. Windows passes
51 suites; Android passes 41 native suites plus updated layout/edit contracts.
User component presets, internal draft viewers, official version updates and
the complete timeline workflow remain outstanding.

The initial typed Point pipeline now includes grid, particle emitter, transform
and texture rendering, with point-node inline previews. Portable fixed-step
simulation reuses focused first-party algorithms and publishes immutable point
snapshots. Shared compiler/package/runtime limits bound source and intermediate
capacities. Two additional templates bring content to 54 presets / 15 templates.
Windows contracts and actual Windows/Android GPU playback pass; exact evidence
and remaining GPU simulation/physics/3D work are in `validation/points_2026-09-07.md`.

The initial Box2D point-physics increment passes 55 Windows suites and seven
affected Android native suites, including actual GLES floor-contact pixels and
device recreation. Windows-published Bouncing Light Rain / Falling Block Pile
packages play on both hosts. The source-generation contract prevents reused
particle IDs from retaining stale bodies after a seed reset. The vcpkg 3.1.1
overlay fixes a reproduced sensor distance defect; SDK configuration verifies
the recorded patch hashes. Content now totals 58 presets / 17 templates. The
Windows deploy folders and Android APK are rebuilt, while APK installation
acceptance remains pending. See `validation/physics2d_2026-09-07.md` for precise
scope; collision-event nodes, joint authoring and 3D remain unfinished.

Room identity is now an isolated validated adapter, with generated P-256 identities,
SHA-256 certificate pins, validity/purpose checks and Windows current-user DPAPI
vaults. Windows and USB Android pass real MsQuic handshakes, wrong-pin rejection
and cancellation using generated identities; Windows also reloads its encrypted
vault before transport use. See `validation/room_identity_2026-09-07.md`.
This does not imply room admission or integration into the application binaries.
The isolated room authority now validates limited invitations, member capacity,
connection binding, lock/kick, credential rotation/renewal and replay watermarks.
Its strict 209-character invitation codec carries endpoint and certificate pin;
Windows/Android contracts include three simulated hours of lease renewal.
See `cluster_invitation_protocol.md`: real encrypted Join/Welcome/Refresh and rejection
now pass on both platforms, while resource/scene transport and camera/App integration remain pending.
Both platforms also pass the shared OpenSSL layout; Windows required an explicit
application-directory/System32 dependency-load profile for the bundled TLS DLLs.
The QR-reader candidate passes bounded luminance decoding and one-worker,
five-per-second scanning with generation cancellation on Windows and Android.
The isolated transport service now exposes project-owned connection/event values,
bounded control/asset streams and realtime datagrams. Nine security/transport suites
pass on Windows and USB Android; the public API also transfers 1 MiB of exact assets
alongside control/datagrams in both actual LAN directions. See
`validation/cluster_transport_2026-09-07.md`; application room/scene integration is
deferred behind the main authoring and local playback work above.

- Texture previews render inside all five texture-producing nodes in the initial
  graph, including final output; dragging an image moves its node.
- Visible preview demand, selected-node priority and an eight-texture ceiling
  replace the fixed three-viewer experiment. Copies remain 256x144 at 15 Hz,
  with no routine GPU-to-CPU readback. Hidden/tiny previews stop requesting work.
- Windows 37 tests pass, including independent Player GPU playback, real GPU inline-preview assertions and clean
  deployment startup. The added canvas tests cover preview visibility and drag.
- Android API 34 / arm64-v8a USB device passes twenty-nine native suites,
  including the expanded preview-resource budget. No Player APK acceptance yet.

Compiled program ABI 2 (with ABI 1 reading) and bounded standard ZIP runtime packages are implemented.
Studio publishes immutable snapshots asynchronously with atomic replacement;
Windows Player independently loads a runtime package, pauses/resumes and restarts.
Shared session tests cover feedback pause, bad-package retention, surface resource
release/replacement, clock continuity and resize. New packages use texture-signal-v2; legacy texture-signal-v1 and
texture-signal-assets-v1 packages remain readable. Media decoding, package signatures and cluster
admission remain separate modules, not implied by these profiles.

Android SDL/Java host, GLES native library and Python APK builder are implemented
and compile. The APK includes native controls, document-picker import and the
same compiled package. USB installation was attempted on e2b3b128 but rejected:
`INSTALL_FAILED_USER_RESTRICTED: Install canceled by user`. The user has been
asked to allow installation on the phone. No GPU/lifecycle acceptance is claimed.
Development continues on independent work while that device prerequisite is pending.
Player now accepts immutable runtime input values at each host frame, with local
clock fallback. Pause and surface replacement retain the last evaluated snapshot;
resume reads current inputs. Invalid values cannot advance the playback clock.
Windows and Android session contracts cover these transitions. No network object
or dependency is introduced into Player/runtime public contracts.
An Android native EGL pbuffer probe already runs the actual bgfx/GLES backend on
the Adreno 650. It exposed and verified a fix for inverted render-target sampling;
device destruction/recreation also passes. This does not replace APK lifecycle tests.

The content module now loads twenty bilingual parameter presets; Inspector applies
resolved values through history, so undo/redo and saved snapshots remain independent
of catalog files. Saving/publishing commits active parameter/title edits first.
Three additional generic operators provide scalar constants, six math modes and
local time with free/loop/ping-pong modes. Enumeration parameters reject fractional
values and use localized menus. Runtime tests cover wrapping, reverse time,
division by zero and unchanged-input caching.

Typed keyframe curves add step/linear/smooth interpolation, endpoint holding,
strict time/value/key-count validation and allocation-free evaluation. A curve
operator composes with local time and scalar/texture inputs. Source projects and
compiled packages serialize the same typed curve; existing key extension fields
survive authoring round trips. Inspector offers a curve plot and clipped keyframe
editor with add/remove, interpolation and live value editing. Headless input tests
cover one-command addition and the 1,024-key limit/visible-row rendering. Parameter
preview/commit ownership is extracted into PropertyInspector rather than growing
the Studio coordinator. This is node-local curve editing, not the complete planned
multi-track timeline or all parameter binding/expression modes.

Media dependency audit: installed Windows FFmpeg 6.1.1 reports GPL version 2 or
later and --enable-gpl/--enable-libx264. The user has been asked whether to select
GPL-3.0-or-later for affected project artifacts or build an LGPL FFmpeg profile.
No GPL FFmpeg integration or outbound-license choice has been made implicitly.

The cluster realtime codec now has a bounded, allocation-free v1 layout for
participant input and request/reply clock datagrams. Golden byte order, all
65,536 quantized control codes, malformed/truncated/extended payloads and a
20,000-packet deterministic mutation corpus pass on Windows and Android. See
`cluster_datagram_protocol.md`; authentication/request matching remain separate.
Isolated QUIC candidates and remaining TLS/transport gates are recorded in
`validation/quic_candidates_2026-09-07.md`, without claiming a completed room.
Clock request matching now retains eight requests with one-second expiry and
locally stamped receive times. The application send queue bounds control/assets
by count and bytes including in-flight payloads, retains only the latest pending
realtime value, and keeps in-flight ownership through cancellation. A 1000-peer
stalled Null queue model passes 120 updates without exceeding its per-peer budget;
this is not a 1000-connection or Wi-Fi capacity test. Full regression: Windows 32,
Android 24 for that increment. Ordered stream framing then adds fragmented/coalesced
message decoding, pre-allocation size checks and receive backpressure. Full
regression now passes Windows 33 / Android 25; device evidence
`/data/local/tmp/rhythm-master-phase-a-20260906213056996`.
The isolated MsQuic adapter now uses the actual shared send queue and stream decoder.
Final callbacks can follow shutdown notification, so native stream/connection close
quiesces callbacks before asserting that in-flight payloads drained. Windows and
Android cancellation races plus both LAN directions pass with these components;
device evidence `/data/local/tmp/rhythm-quic-lan-20260906213421`.

Scene readiness/commit coordination now rejects stale generations/hashes/profiles,
freezes initial membership by the preparation deadline, supports all-ready or
ready-subset policy, and issues a separate future commit for late-ready peers.
Peer and generation identities are bounded and never reused. These are host
domain tests, not a completed authenticated room or measured display sync.

Move-only PreparedPackage validates/digests off-thread and transfers exactly once
into Session. PackageLoader reuses one bounded worker for file reads, validation
and optional atomic install, with cancel/exit/error recovery. Windows Player open
and Android private-cache import now use it while current playback continues.
Android retains one active and one replaceable pending imported cache file;
worker join precedes RAII deletion, and paths outside the private incoming-file
namespace are rejected and retained. Both platform tests cover these boundaries.
Full regression: Windows 36 / Android 28, device directory
`/data/local/tmp/rhythm-master-phase-a-20260906215311581`. APK installation and
actual Android foreground/background document-picker acceptance remain pending.

RuntimePackage now retains its validated profile. PreparedPackage conservatively
allows analytic time evaluation only for the known stateless operators; feedback
and future unclassified operations cannot bypass history recovery. Session supports
a validated initial offset that survives pause/surface replacement and clears on restart.
`cluster_player::PlaybackSchedule` connects prepare/hash/profile validation with
frame-boundary Session commits, rejects stale async completions and missed timing,
and preserves the scene origin for analytic late join. It is a separate service,
without transport types or dependencies in standalone runtime/render. Full
regression: Windows 37 / Android 29, device directory
`/data/local/tmp/rhythm-master-phase-a-20260906220528804`. Actual room UI, authenticated
admission/transport wiring and multi-screen synchronization remain unimplemented.

Isolated real QUIC connection bursts now pass Windows 10/100/1000 concurrent
loopback connections and Android 10/100, including over-limit rejection and
native-close buffer drain. Exact timings, process CPU/RSS accounting and artifact
hashes are recorded in `validation/quic_candidates_2026-09-07.md` and
`out/quic/stress-*.json`. These are same-process client/server transport bursts,
not sustained full-room load, Wi-Fi capacity or mobile-host product support.

Build paths:
- Windows Studio: `out/windows/src/windows_spike/deploy/rhythm_master.exe`
- Windows Player: `out/windows/src/windows_player/deploy/rhythm_player.exe`
- Android local debug APK: `out/android-arm64/apk/rhythm-player-debug.apk`

Next: complete Android real GPU/lifecycle/package tests when installation is
allowed; continue remaining capability, media, assets and cluster plan modules.

Evidence: `out/windows-build.log`, `out/android-build.log`,
`out/android-device.log`, `out/android-apk-build.log`, `out/studio-nodes-full.png`.

Asset increment: immutable SHA-256 blobs now import with streaming size limits,
deduplication, durable staging, corruption checks and cancellation. Studio offers
a background import/record removal panel with undo/redo. Snapshots and project
revision manifests carry only IDs, sizes and media types; missing/corrupt assets
prevent commit. Publishing through Studio or the CLI carries verified bytes in
separate ZIP entries. The initial in-memory profile limits assets to 64 records
and 8 MiB total, within the existing 16 MiB package cap; streaming large media
packages and graph media consumers are still pending.

Asset tests cover source deletion, stable deduplication, mid-copy cancellation,
failed-save retention, missing/extra/corrupt ZIP entries and bounded expansion.
An independent Python ZIP reader verified a CLI-published asset package. That
Windows package then played 60 frames on the Android Adreno 650 using the actual
GLES backend. Evidence: out/android-assets-gpu.log and out/windows/package-cli-test.rhythmpack.

Signal/format increment: sixteen generic operators now include bounded range
mapping, six comparisons with equality tolerance, conditional scalar selection
and deterministic seeded step/linear/smooth noise. Seventeen bilingual presets
include validated defaults. Fixed noise samples match on Windows and Android.
Property editing uses double precision and integer seed controls.

An allocation-free Protobuf preflight bounds repeated node/edge/property/keyframe
fields, packed/unpacked slots, nesting, total field count (200,000) and total
message count (100,000) before generated objects allocate memory. Wire tests
cover duplicate singular messages, malformed varints and unknown groups.

Canvas sizes now belong to graph/program values. Source schema 2 and program
ABI 2 require the canvas; legacy schema/ABI 1 reads as 640x360. New packages use
texture-signal-v2 and verify matching canvas metadata, with optional asset entries.
Studio provides compact landscape, 720p landscape/portrait and square options
through history. The current profile permits dimensions 16..4096 and at most
2,073,600 pixels. Studio output, Windows/Android Player and node previews keep
aspect ratio. A Windows-published 720x1280 package with an asset completed
60 actual Android GLES frames (out/android-portrait-gpu.log).

Template increment: the default landscape project and new Twilight Flow portrait /
Square Pulse templates are data-authored, compiled and published automatically.
Studio's bilingual template menu loads on a bounded worker, verifies/copies assets,
rejects concurrent edit conflicts and replaces content as one undoable command
with fresh node IDs. Localized default selection lives in template metadata.
Windows 24 suites and Android 16 native suites pass; both additional Windows-built
templates complete real Windows (30-frame) and Android (60-frame) GPU playback.
Evidence: out/android-template-gpu.log. These three experimental examples do not
meet the complete 24-template / 40-semantic-node content targets.

Windows Studio accepts --project <directory> and Player --package <file>, including
Unicode paths; --smoke can combine with either for isolated acceptance. Actual
portrait Studio output and node-preview framing: out/studio-portrait.png.

Cluster foundations now include a value-only clock estimator with bounded drift,
slewed monotonic presentation time, epoch/replay rejection and one-hour synthetic
drift tests. A fixed 32-frame participant input buffer interpolates controls,
steps discrete roles, and expires stale data through hold/fade. Nineteen generic
operators include session time, participant role and control channels. Studio's
offline input panel exercises these contracts without pretending to join a room.
Authenticated transport, invitations and actual multi-device display sync remain pending.

GammaRay's first-party bounded executor and deferred thread joiner are extracted
with exact source hashes in provenance/gammaray_common.json. No Asio was imported.
Project save/publish/template load and asset import reuse bounded workers, publish
value results, and drain or cooperatively cancel on destruction. Windows 28 suites
and Android 20 native suites pass after integration, including reentrant capture
cleanup, worker-owned shutdown, queue saturation, repeated imports and failed-job
recovery. Android evidence: /data/local/tmp/rhythm-master-phase-a-20260906200008828.

QR generation is extracted with separate first-party wrapper and MIT Nayuki
provenance, bounded payload/pixels, quiet zone and integer scaling. Windows and
Android images match; independent decoding passes 20 synthetic image cases.
Complete builds now pass 29 Windows and 21 Android native suites. Camera scanning,
invitation validation and authenticated join remain separate work; see
validation/cluster_foundations_2026-09-07.md for exact evidence and limits.


## Latest continuation: bounded rendering and Android complex-scene evidence

Resource admission failures now keep Studio/Player alive and permit explicit or
edited-scene recovery. Real D3D11 tests cover memory, framebuffer handles, pass
exhaustion and reopening a smaller canvas. See
[render budget recovery](validation/render_budget_recovery_2026-09-07.md).
The 164-node musical Resonance Gate also runs in the Android native GLES probe;
720p/540p/360p p95 are 45.95/34.07/17.43 ms. This is synthesized feature input,
not Android APK real-music acceptance or a 60 fps claim. Full content/product
milestones remain open; communications remain deferred.


## Product benchmark clarification — 2026-09-08

The user confirmed TiXL and TouchDesigner as the target class of software.
[Product scope](product_scope.md) now makes original node-based authoring,
live intermediate previews, composable audio/signal/parameter/time inputs,
complex visual domains, reusable components and complete output workflows
explicit acceptance dimensions. Music visualization remains the core use case;
wallpaper hosting remains excluded. Template counts and standalone playback do
not establish completion of Studio authoring. Existing performance, media,
platform and deferred-communication priorities continue under these criteria.

## 2026-09-08 音画编码后端

Windows MPEG-4/AAC、H264/MF 与 Android 原生 MPEG-4/AAC 回读通过；
准确处理 MP4 movie timescale 和 AAC 末帧填充。导出 UI/GPU 队列仍在实施，
见 [验证记录](validation/media_encoding_2026-09-08.md)。

## 2026-09-08 有界异步读回

Windows D3D11 与 USB Android/GLES 的三帧异步读回、像素顺序、透明度和取消通过。
Viewer 无 CPU 读回；离线导出任务继续接入。见 [验证记录](validation/async_readback_2026-09-08.md)。

## 2026-09-08 离线音画导出核心

197 节点作品与真实音乐已生成 4 秒 MP4，同机重复导出逐帧一致，音轨长度准确；
静音改变视觉，取消与预算验证通过。导出 UI/进程/发布继续接入，
见 [验证记录](validation/offline_export_core_2026-09-08.md)。

## 2026-09-08 Studio 音画导出闭环

导出面板、独立隐藏 host、有界编码线程、进度/取消和禁止覆盖的原子发布已接通。
真实 Studio 按钮流程导出 16 秒 H.264 MP4，父编辑器 p95 帧耗时 16.95 ms；
13 项回归通过。Windows 部署已更新，Android 共享核心编译及真机原子发布通过。
见 [完整规格与验证](validation/studio_export_2026-09-08.md)。
作品音乐绑定持久化、媒体编排、50 + 50 品质内容与 Android 应用验收继续推进。

## 2026-09-08 作品音乐持久化和跨平台播放

单曲绑定、音量/循环、撤销、保存/恢复和携带音乐的运行包已接通，Windows Player
自动读取包内音乐。真实 Studio 操作生成的 197 指令作品已在 USB Android/GLES 上
用真实 PCM 与静音做像素对照，差异明确，纹理内存稳定；640×360 的 p95 为 19.25 ms。
见 [规格与证据](validation/work_soundtrack_2026-09-08.md)。当前共享资产上限仍为 8 MiB，
大媒体流式容器、编排、100 个品质模板与 Android 应用验收仍未完成。

## 2026-09-08 数值与信号节点内预览

标量、频段、时间和表达式节点可显示运行值与有界采样曲线，组件内部沿用实例映射。
实际图运行、暂停/跳转、曲线区域拖动、千节点交互以及 USB Android 共享核心测试通过；
真实 Studio 的 210 帧组件预览维持合计八个预览上限。未增加 GPU 预算或引入第二套绘图库。
见 [行为边界与证据](validation/signal_previews_2026-09-08.md)。

## 2026-09-08 大音乐文件读取前置验证

共享文件范围、32 KiB FFmpeg 自定义 I/O、精确 PCM/跳转和 Windows 持续读取中的
原子文件替换已验证；14 项受影响回归及 Android 真机读取/解码/发布通过。
见 [文件音频范围记录](validation/file_audio_ranges_2026-09-08.md)。本步骤尚未改变运行包
和作品绑定上限，继续接入流式容器、资源准备与 Player。

## 2026-09-08 大音乐作品发布与播放

`music-performance-v2` 已接通单首 256 MiB 音乐、8 MiB 普通素材和 272 MiB 文件包，
沿用 miniz、PicoSHA2 和同一 FFmpeg 文件范围解码。128 秒、24 MiB 原创音乐完成
Studio 绑定/保存/发布/重开、中英文交互、后台安装、Windows 与 Android 全曲 PCM
比对。手机用同一 Studio 作品完成 480 帧音乐/静音 GLES 对照，p95 19.10 ms，纹理
内存稳定；这不代表稳定 60 fps。见 [大音乐运行包验证](validation/large_music_packages_2026-09-08.md)。
最新版 APK 与重链接材料已生成，手机仍以 USB 安装限制拒绝安装，应用生命周期门
保持未通过。媒体编排、100 个品质模板与 Android 完整验收继续实施。

## 2026-09-08 整曲波形与时间线定位

时间线可查看整首音乐的有界峰值波形、秒刻度和播放指示线，鼠标松开时通过既有
音乐时钟提交一次定位。扫描使用同一 FFmpeg 解码器和可取消工作线程；更换音乐、
关闭时间线不会让旧结果覆盖当前曲目。128 秒音乐的逐采样包络、中英文真实交互、
Studio 实际场景与 USB Android 共享模块验证通过，见
[整曲波形验证](validation/music_waveform_2026-09-08.md)。段落控制和多轨编排继续推进。

## 2026-09-08 可编辑时间段落

显式时间输入的 `time.envelope` 已支持区间、淡入淡出和线性/平滑形状。时间线可在
播放位置添加、拖动和裁剪段落，每次手势一次撤销；输出连接到图参数。中英文实际
交互、197 指令 Studio 保存/发布/恢复和 USB Android 共享核心及 GLES 音乐对照通过。
见 [段落控制验证](validation/time_sections_2026-09-08.md)。组件局部时间线、多轨音频、
事件轨、100 个品质模板与 Android 应用验收仍未完成，不能据此宣称完整编排已完成。

## 2026-09-08 四段音乐编排示例

“星门编排”复用星门演出的两个组件，以四个段落控制开场、推进、高潮和收束，
共 208 条可达指令。真实 Studio 完成绑定/保存/发布/恢复、16 秒 H.264 导出；
独立离线导出重复帧哈希一致，手机完成四段音乐/静音像素对照与 1,920 帧运行。
见 [编排示例与观感修正](validation/resonance_arrangement_2026-09-08.md)。目录现有
31 个功能/视觉示例，此复用编排不计入独立 Basic/Advanced 品质模板数量。

## 2026-09-08 组件内部曲线与段落编辑

组件工作台已接入与根时间线共用的曲线、段落草稿编辑器，支持指定时间添加、
移动/裁剪、内部撤销与应用到工程；保留显式局部时间连线，不增加播放时钟。
中英文真实交互、原有组件编辑、根时间线与 D3D11 组件预览回归通过，见
[组件时间编辑](validation/component_timing_2026-09-08.md)。多轨音频、事件与完整
历史预演仍是后续能力，Android 应用验收仍需解除手机 USB 安装限制。
