# Commercial-grade open-source product and release plan

> Status: product constraints and release gates; no product release gate has passed
>
> Date: 2026-09-06

## 1. Purpose

Rhythm Master is an open-source real-time node-based visual authoring product,
benchmarked against TiXL and TouchDesigner, with music visualization as its core
use case. Studio enables original compositions and reusable components; Player
executes the resulting published works.
[Product scope](product_scope.md) governs its workflows and acceptance; desktop
wallpaper hosting is excluded on every platform. Commercial-grade describes its quality:
reliable operation, responsive editing, reproducible builds, maintainable code
and complete user workflows. This plan defines delivery, recovery, release and
support requirements. Paid editions, activation and proprietary licensing are
not requirements. The historical filename is retained for existing links.

Product scope is split deliberately:

- **Rhythm Master Studio**: authoring, node editing, inspection, profiling,
  asset management and publishing on Windows and macOS;
- **Rhythm Master Player**: validated package playback on Windows, macOS,
  Android and iOS;
- **shared portable runtime**: graph evaluation, effects, audio analysis,
  physics, rendering, package validation and deterministic tests;
- **platform hosts**: lifecycle, windows/surfaces, input, audio devices, safe
  storage, signing, store services and platform-specific presentation.

Android and iOS provide music visualization Players. Their acceptance covers
real music input, audio-driven output, playback controls and application lifecycle.
Native transparent-window features retain their separate tentative/deferred status.

Local cluster playback is now in scope: desktop Host, native Player QR join,
shared scene/time/audio inputs and optional group roles. It is not cloud graph
collaboration, default video streaming or a paid service. See
`cluster_playback_plan.md` for admission, security, device lifecycle and measured
venue-capacity gates. Core network work is tested on Windows first; real mobile
participation follows the Windows acceptance gate.

## 2. Product decisions required before architecture freeze

The following decisions affect persisted formats, module boundaries and release
infrastructure and therefore must be made before broad implementation:

1. first-release user profiles and delivery details for the music input → authoring/preview → playback/export/publishing workflows in `product_scope.md`;
2. exact Studio publishing and offline export formats;
3. project outbound license and compatibility of distributed dependencies;
4. local/offline workflows, with accounts optional for any future hosted service;
5. official distribution channels for each platform;
6. whether third-party executable plugins are forbidden, sandboxed or signed;
7. supported operating-system, GPU and driver baseline;
8. telemetry and crash-report consent policy;
9. supported languages at launch and the translation ownership process;
10. support lifetime and compatibility promise for published packages.

Every release has explicit non-goals. A feature that is absent from a release
must not be implied merely because a low-level engine primitive exists.

## 3. Distribution, signing and updates

### 3.1 Desktop Studio and Player

Windows delivery requires:

- signed application binaries, installer and update metadata;
- install, repair and uninstall flows without deleting user projects;
- stable and preview release channels;
- file associations for authoring projects and published packages;
- atomic update staging, signature verification and automatic rollback;
- a portable-build policy distinct from the installed product;
- startup detection of an interrupted or incompatible update.

macOS delivery requires:

- universal or explicitly separated architecture artifacts;
- application signing, hardened runtime and notarization;
- declared filesystem/media permissions and sandbox strategy;
- atomic update or store-controlled update behavior;
- preservation of projects and recovery data across upgrades.

### 3.2 Mobile Player

Android and iOS builds require store-compliant signing, package acquisition,
safe storage and lifecycle behavior. Published effects are application data,
not dynamically loaded executable code. Package download and installation must
verify identity, integrity, size limits and required runtime capabilities.

### 3.3 Supply-chain requirements

- pin all source and binary dependency versions;
- generate an SBOM and third-party notice bundle per release;
- record compiler, dependency and build configuration provenance;
- make release builds reproducible where practical;
- sign release manifests and retain rollback artifacts;
- keep credentials outside source code and developer project files.

## 4. Reliability and recovery

Release builds require:

- structured rotating logs with redaction rules;
- native crash dumps and an opt-in crash-report path;
- project autosave using independent recovery revisions;
- session recovery after process, power or GPU failure;
- safe mode that disables the last project, custom shaders and optional
  extensions;
- renderer device-loss detection and controlled resource recreation;
- startup rollback after repeated failure;
- a one-action diagnostic bundle containing versions, GPU/driver capabilities,
  relevant logs and sanitized project validation results;
- low-disk, corrupt-data and partial-download handling.

Studio must never overwrite the last valid project revision during migration or
publishing. Player must fail a bad package without making the application or
the next launch unusable.

## 5. Content and execution security

Projects and published packages are untrusted input. Loaders enforce:

- schema, feature-level, hash and path validation before resource creation;
- file-count, byte-size, image dimension, graph-depth and nesting limits;
- protection against path traversal, archive bombs and duplicate-name tricks;
- bounded Protobuf parsing and fuzz-tested import paths;
- media/model decoder limits and cancellation;
- shader source, include and resource-access policy;
- asynchronous shader compilation with timeout and failure isolation;
- GPU workload budgets and recovery from an invalid render graph;
- signed update/package provenance when content is obtained remotely.

No graph node may execute an arbitrary process, load a native library or read an
unapproved filesystem path in the initial product. A future plugin SDK requires
a separate versioned API, permission model, signing policy and crash boundary.

## 6. Export and publishing product

Studio publishing produces a validated `.rhythmpack` for all compatible Player
targets. Validation reports unsupported nodes, shaders, texture formats,
precision requirements and memory estimates per target platform.

Desktop offline export is a separate pipeline and must define:

- image sequence and video container support;
- resolution, orientation, frame rate and duration;
- alpha, SDR/HDR and color-space behavior;
- deterministic timeline stepping and seeded simulation;
- audio decoding, synchronization and optional muxing;
- encoder availability, licensing and hardware/software fallback;
- cancellation, progress, temporary storage and atomic finalization.

Editor preview, exported media and Player output use one documented color
pipeline. Golden-image tests cover sRGB/linear conversion, transparency and
tone mapping.

## 7. Asset and project workflow

Studio requires:

- content-addressed import, deduplication and dependency tracking;
- missing-asset detection and deterministic relinking;
- proxy/thumbnail caches that can be deleted and rebuilt;
- asset license/source metadata for bundled or redistributed work;
- package-size and target-capability analysis before publish;
- recent projects, templates, examples and recoverable deletion;
- one-time import of supported projects from the old application;
- human-readable validation and migration diagnostics.

The SQLite catalog remains a rebuildable index. It is not a source of truth and
may be deleted without losing a project or installed package.

## 8. Quality and compatibility gates

The release matrix covers at least:

- supported Windows and macOS versions;
- integrated and discrete GPUs from supported vendors;
- current and minimum supported drivers;
- Retina/high-DPI, 4K, multiple monitors and mixed display scaling;
- portrait, landscape and unusual design resolutions;
- audio devices, sample rates, device changes and interruption;
- suspend/resume, display disconnect and renderer device loss;
- restricted permissions, Unicode paths and low storage;
- representative Android/iOS GPU, memory and thermal classes for Player.

Automated gates include:

- headless graph/compiler/runtime tests;
- cross-platform execution of the same published conformance packages;
- deterministic image and numeric reference tests;
- loader fuzzing and corrupt-package tests;
- save interruption and migration tests;
- UI input replay for critical Studio workflows;
- frame-time, memory and GPU-resource regression thresholds;
- long-running playback and repeated open/close stress tests.

UI, graph evaluation, node viewers, final output and presentation have separate
budgets. Final output has higher priority than optional node previews.

## 9. Product experience

The visual-programming capability and built-in content requirements are defined
by `visual_authoring_capability_plan.md` and
`builtin_nodes_and_presets_plan.md`. A production release must satisfy their
operator, semantic-node, preset and template gates; a renderer primitive alone
is not counted as a finished user-facing feature.

Before public beta, Studio needs:

- first-run setup and language selection;
- a versioned built-in semantic-node/preset library, template projects and a
  guided first publish;
- consistent undo/redo, keyboard navigation and shortcuts;
- searchable node creation and command palette;
- clear empty, loading, error and recovery states;
- inspector validation located beside the invalid property;
- resource relink and package compatibility UI;
- integrated performance and graph diagnostics;
- versioned documentation matching the installed release.

Player needs a deliberately smaller interface: library/acquisition, package
validation, playback controls, display/output choice, audio source, language,
diagnostics and recovery. It must not expose partially functional authoring UI.

Accessibility requirements include scalable UI, minimum contrast, visible
focus, keyboard-only core workflows and color-independent diagnostics. Dear
ImGui accessibility limitations are treated as product work, not as an assumed
framework feature.

## 10. Localization

Studio and Player are multilingual products. Localization follows
`localization_plan.md`; core requirements are:

- stable message IDs and UTF-8 boundaries;
- Simplified Chinese and English as initial complete locales;
- locale fallback, plural/parameter formatting and font fallback;
- tested IME behavior in Studio;
- localized diagnostics, recovery flows, legal notices and Player UI;
- optional localized effect metadata without localized runtime identifiers.

## 11. Legal and licensing

Use `third_party_reuse_policy.md`. GPL and other copyleft dependencies are
eligible for source reuse. Select the project's outbound license before an
integration that constrains distribution, and retain third-party notices and
corresponding-source obligations. No closed-source compatibility constraint is
assumed.

Before distributing any build, audit and record:

- SDL3, bgfx/bx/bimg, Dear ImGui, imgui-node-editor, Box2D, Protobuf and all
  transitive source dependencies;
- FFmpeg build options, codec linkage, redistribution and source obligations;
- fonts, icons, models, textures, music and template/demo licenses;
- shader source provenance;
- store, Steam and platform SDK terms;
- project license, contribution terms, trademark policy, privacy policy and
  third-party notices; any service terms must respect open-source license rights.

The build must generate the exact notices for the dependencies and assets in
that artifact. Sample content is not assumed redistributable merely because it
is present in a development repository.

## 12. Privacy, operations and support

Telemetry and crash reports are opt-in unless platform policy and published
privacy terms explicitly establish another lawful basis. Collected fields,
retention, redaction, export and deletion must be documented. Projects, media,
paths and graph contents are not uploaded implicitly.

Operational planning covers:

- release-channel and update-manifest hosting;
- crash symbol storage and access control;
- incident response and bad-release withdrawal;
- customer support intake and diagnostic-bundle handling;
- compatibility/deprecation announcements;
- backup and restore of signing and release infrastructure;
- version support lifetime and security-fix policy.

Local Studio and Player workflows must work without product activation. Cloud
synchronization, collaboration, optional hosted accounts and third-party plugins
are later feature decisions. Pricing, trial editions and marketplace sales are
not part of the currently requested plan.

Open-source delivery includes public build instructions, contribution guidance,
an issue workflow, dependency provenance, and matching source/build materials
for released artifacts as required by their licenses.

## 13. Delivery gates

### Gate A: architecture freeze

The already-authorized Phase A experiments precede this full freeze; see
[the execution plan](phase_a_execution_plan.md) for the shared-platform P0–P4
and first-host W0–W5 gates. A working Windows slice does not close this gate,
establish multi-platform support or select the outbound license.

- open-source licensing, launch workflows and non-goals approved;
- platform matrix and shared Player boundary approved;
- project/package, localization and trust models approved;
- dependency/license inventory created;
- performance and compatibility baselines defined.

### Gate B: private Windows alpha

- risk-first vertical slice passes architecture acceptance;
- save/recovery, package validation and diagnostic logging function;
- one complete author-publish-play-export workflow passes;
- representative large graph stays within measured budgets.

### Gate C: Windows public beta

- signed installer and rollback-capable updater;
- crash reporting, safe mode and recovery UI;
- supported GPU/driver test matrix passes;
- complete Simplified Chinese and English UI;
- legal notices, privacy policy and support workflow ready.

### Gate D: Windows release

- migration, export, soak, corrupt-input and upgrade tests pass;
- release artifacts, symbols, SBOM and rollback artifact retained;
- no unresolved data-loss, security or release-blocking performance issue.

### Gate E: Android Player

- only portable runtime and Player host dependencies are present;
- shared conformance packages pass within mobile resource budgets;
- lifecycle, interruption, permissions, signing and store requirements pass;
- Studio/editor code is absent from mobile artifacts.
- cluster join, permission denial/revocation, reconnect and thermal behavior
  pass on real devices; publish device/network capacity separately from
  simulated-peer throughput, never as an unsupported universal N-device claim.

### Gate F: final Apple platform delivery

By the user's 2026-09-07 decision, Apple is last because no Apple hardware is
currently available. Its missing execution evidence does not block earlier
Windows/Android gates; portable contracts continue to include Apple targets.

- macOS Studio/Player parity is measured against accepted Windows workflows;
- Apple shared-target builds, Metal, Retina, IME, lifecycle, signing and notarization pass;
- iOS Player meets the lifecycle, permissions, GPU, package and cluster checks
  above on its own devices, plus iOS signing/store requirements;
- the same project and published package behave consistently on validated targets.

## 14. Technical decision register

Use `technology_stack_evaluation.md` for confirmed/candidate dependencies and
the four risk-first validation groups. Library version, platform support,
codec/profile, licensing and benchmark evidence are release inputs, not
assumptions inherited from a reference repository. FFmpeg and Godot decisions
are detailed in `media_pipeline_plan.md` and `godot_3d_reference_plan.md`.
