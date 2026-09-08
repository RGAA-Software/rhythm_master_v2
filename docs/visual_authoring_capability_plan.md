# Visual authoring capability plan

> Status: partially implemented; this document describes the target, not completion.
> Current code-based gaps and feature-first order: [2026-09-08 review](feature_gap_review_2026-09-08.md).
> Current execution sequence: [rendering roadmap](rendering_capability_roadmap.md).
>
> Date: 2026-09-06
>
> Scope: Studio on Windows/macOS and the shared Player runtime on
> Windows/macOS/Android/iOS

## 1. Decision

Rhythm Master is not migrating the old fixed-effect editor into a new UI. It is
building a new typed visual-programming system using the old renderer, audio,
physics and effect code only as audited implementation material.

The user confirmed on 2026-09-08 that TouchDesigner and TiXL are product-level
benchmarks: authoring freedom, node interaction, live feedback, parameter/time
control, component reuse and complete output workflows all matter. This is a
capability target, not a claim of current parity or adoption of every upstream feature.

The target is the rendering freedom of TouchDesigner and TiXL in a product that
is easier to learn for music visualization and audio-driven motion graphics
authoring and playback. [Product scope](product_scope.md) governs these capabilities;
desktop wallpaper hosting is excluded. Hardware protocols, general application
scripting and arbitrary UI application construction are not initial parity goals.

Success requires all of the following, not merely a large node count:

- composable texture, signal, point/particle, geometry, scene and material data;
- explicit bridges between those domains;
- demand-driven, incremental and time-aware execution;
- observable intermediate results and a protected final-output path;
- parameter binding, safe expressions, keyframes and local clocks;
- reusable semantic components with curated public controls;
- a large data-driven preset and template library;
- deterministic compilation to portable Player packages.

## 2. Reference corpus and legal boundary

The following local source snapshots are research and implementation-reuse
candidates. Rhythm Master is open source and aims for commercial-grade quality.
Apply `third_party_reuse_policy.md`; copyleft is not an exclusion criterion.

| Project | Local revision | License/use boundary | Primary lesson |
| --- | --- | --- | --- |
| TiXL | `fbc994d923e8a0142d2ff1b772e4d12248c5b0ba` | MIT; implementation may be studied after file-level audit | procedural operators combined with linear keyframes, compute/fragment shaders, parameter exploration and standalone output |
| cables | `75d9960b75545cbd40037d4b48ccf52ac8c75b63` | MIT metadata; audit individual packages | operator packaging, dynamic ports, subpatches, namespaces, documentation and self-contained patch export |
| Coollab | `5d1281d8d5a1018a52e279d8873c206410a71f4b` | GPLv3-family candidate; source study, reuse and adaptation allowed subject to exact file terms, compatible artifact licensing and source obligations | source-plus-modifier workflow, immediate good defaults, hundreds of effects, audio accessibility and per-node presets |
| Material Maker | `ad19fcf0ee34a7caf74df709dc4de7112f0d467d` | MIT; implementation may be studied after file-level audit | typed visual data, automatic conversion, previewable outputs, expressions, seeds, variadic nodes and editable templates |
| ossia score | `098de7c501b390f1f438c64e4d8ba20001671f7d` | GPLv3-family candidate; source study, reuse and adaptation allowed subject to exact file terms, compatible artifact licensing and source obligations | scenarios, states, automation, process composition, drag-to-connect workflow and threaded dynamic render graphs |

TouchDesigner remains a public-documentation and observable-behavior reference.
This policy does not grant access or copying rights to proprietary sources or
assets. Root license labels above are initial inventory entries, not a substitute
for checking the exact files, exceptions and transitive dependencies reused.

Reuse decisions compare portability, quality, tests and maintenance cost. They
may include focused source imports, shader adaptation and maintained forks. The
zero-Qt, RhythmRender and desktop/mobile dependency contracts remain mandatory.

Reference material:

- [TouchDesigner operator families](https://docs.derivative.ca/Operator_Family)
- [TouchDesigner parameters and operator basics](https://docs.derivative.ca/Getting_started)
- [TouchDesigner components](https://docs.derivative.ca/Component)
- [TouchDesigner Time COMP](https://docs.derivative.ca/Time_COMP)
- [TouchDesigner Animation COMP](https://docs.derivative.ca/Animation_COMP)
- [TouchDesigner Palette](https://docs.derivative.ca/Palette)
- [TiXL](https://github.com/tixl3d/tixl)
- [cables subpatch operators](https://cables.gl/docs/5_writing_ops/patchingops/subpatchops)
- [cables standalone patch export](https://cables.gl/docs/4_export_embed/dev_embed/export_standalone/export_standalone)
- [Coollab presets](https://coollab-art.com/Tutorials/Writing%20Nodes/Presets)
- [Coollab audio workflow](https://coollab-art.com/Tutorials/VJ/Audio)
- [Material Maker node model](https://rodzill4.github.io/material-maker/doc/nodes_common.html)
- [Material Maker node library](https://rodzill4.github.io/material-maker/doc/base_library.html)
- [ossia score processes](https://ossia.io/score-docs/processes.html)
- [ossia score graphics pipeline](https://ossia.io/score-docs/in-depth/video.html)

## 3. Lessons adopted from the references

### 3.1 TouchDesigner

Adopt:

- distinct but bridgeable operator domains rather than one untyped `Any` graph;
- generator, filter, composite and output roles;
- a viewer on every meaningful data-producing operator;
- constant, expression, export/binding and reference-style parameter modes;
- hierarchical components containing networks;
- local component time as well as project time;
- cooking/dirty-state visibility and bypass/cache/lock debugging concepts;
- a palette of reusable components separate from primitive operators.

Do not initially adopt:

- the full DAT/script environment;
- hardware/protocol breadth such as DMX, NDI, tracking and industrial devices;
- user-authored desktop UI panels as a general application framework;
- legacy and overlapping operator families where a smaller typed model is
  clearer.

### 3.2 TiXL

Adopt:

- procedural composition and linear animation as equal first-class workflows;
- parameter exploration, keyframe curves, automation and audio reaction;
- thumbnails and focused help close to operator creation;
- fragment/compute extensibility behind a validated shader contract;
- packaging a composition for a standalone playback runtime.

Do not inherit its Direct3D-specific public model. RhythmRender and portable
shader products remain the backend-neutral boundary.

### 3.3 cables

Adopt:

- declarative operator metadata and independently versioned operator packages;
- typed/dynamic ports and creating a compatible node directly from a cable;
- subpatch creation from a selection and explicit public subpatch ports;
- namespace rules separating core, official, user and future extension content;
- export containing the graph, required assets and custom subpatch definitions.

### 3.4 Coollab

Adopt the product principle that every new node produces a useful result with a
good default configuration. Every artist-facing node has at least one tested
default preset, bounded controls and short task-oriented help. Audio features
are exposed as understandable signals instead of requiring FFT knowledge.

### 3.5 Material Maker

Adopt:

- colored typed ports and explicit, safe automatic conversions;
- selecting which output of a multi-output node is previewed;
- parameter metadata containing bounds, units, widgets and descriptions;
- deterministic random seeds with inheritance and explicit locking;
- variadic ports for real variable-arity operations;
- named public parameters for a reusable subgraph;
- making an official node editable as a local copy;
- target-specific export profiles.

### 3.6 ossia score

Adopt:

- timeline intervals, cues/states and automation as a layer over the graph;
- drag-and-drop creation and automatic compatible connection;
- full-size focused editing for curves or nested content;
- render graph execution isolated from high-frequency editor interaction;
- portable shader/effect metadata and reusable execution/automation code under
  compatible open-source terms; Qt-bound pieces require adaptation to the
  project's platform-neutral contracts.

## 4. Target domain model

Rhythm Master uses fewer, clearer domains than TouchDesigner while retaining
the cross-domain power needed for visual work.

| Domain | Core value examples | Purpose |
| --- | --- | --- |
| Value | scalar, integer, boolean, vector, color, transform | ordinary parameters and calculations |
| Signal | sampled scalar/vector channels, spectrum, waveform | time-varying control, audio and automation |
| Event | pulse, onset, lifecycle, collision | discrete triggers without abusing float thresholds |
| Texture | Texture2D, TextureCube, depth, mask | GPU image generation, filtering and composition |
| Point | typed GPU attribute buffers | particles, trails, instances, point clouds and fields |
| Geometry | mesh, path, shape/SDF | 2D/3D geometry generation and modification |
| Material | surface/shader program plus typed bindings | reusable appearance independent of geometry |
| Scene | Scene2D, Scene3D, camera/light collections | hierarchical renderable composition |
| Resource | image, video, font, model, shader and package asset IDs | immutable asset references |
| Output | preview, fullscreen playback, capture, published target | demand roots and music visualization output |

Ports do not use an unrestricted `Any` type. Conversions are registered,
costed and visible. Lossless conversions may be inserted automatically; lossy,
readback or domain-changing conversions require an explicit bridge node. For
example, Signal-to-Texture is a GPU upload/packing operation and Texture-to-
Signal is normally delayed asynchronous readback, so neither is represented as
a free scalar cast.

## 5. Operator definition contract

Every operator is described independently of Dear ImGui and the concrete render
backend. Its definition includes:

- stable type ID and schema version;
- localized title, summary, keywords and documentation IDs;
- category, maturity and ownership namespace;
- typed fixed and dynamic ports;
- parameters with type, default, range, unit, widget and public-exposure rules;
- evaluation class: pure CPU, stateful CPU, GPU pass, compute, resource, bridge
  or structural component;
- dirty inputs and output invalidation rules;
- time dependence, state slots and deterministic seed behavior;
- optional preview descriptors per output type;
- target capability requirements and fallback policy;
- cost hints for resolution, memory and update frequency;
- migration functions for prior schema versions;
- diagnostic codes and focused conformance tests.

Operator definitions are organized in focused packages. There is no universal
constants header or giant registry implementation. Registration consumes
descriptors from domain packages and builds immutable indexes for the compiler,
editor palette and documentation generator.

## 6. Evaluation and render semantics

### 6.1 Demand and dirty propagation

Final outputs, pinned viewers, exports and explicit probes are demand roots. The
compiler removes unreachable work and builds an immutable execution plan.
Runtime evaluation distinguishes:

- value revision changes;
- resource revision changes;
- structural graph changes;
- time-dependent operators;
- event-driven operators;
- stateful simulation and feedback.

Dragging a node or moving the graph camera never recompiles or evaluates the
effect. Parameter patches update only affected state. Structural edits compile
off the render thread and exchange plans at a frame boundary.

### 6.2 Time and state

The runtime supports project time and nested local clocks. A local clock can
inherit, scale, offset, loop, hold or run independently. Fixed-step simulation
is separate from presentation frames. Offline export advances deterministically
without depending on wall-clock speed.

Cycles are illegal except through explicit Delay, Feedback or state operators.
Those operators declare history lifetime, reset behavior, resize policy and
seed. Stateful objects retain stable IDs across compatible incremental edits.

### 6.3 Render graph

Texture, point, geometry, material and scene operators compile into one render
program containing passes, resource lifetimes and barriers. It supports:

- transient texture/buffer aliasing;
- pass culling from demand roots;
- render-target groups and subgraph composition;
- compute and graphics dependencies;
- asynchronous asset/shader preparation;
- platform capability validation;
- final-output priority over optional previews;
- per-pass timing, memory and dependency diagnostics.

No routine node viewer performs GPU-to-CPU readback. All viewers sample existing
RhythmRender resources through stable texture handles.

## 7. Parameter modes and automation

Every animatable parameter has one explicit mode:

1. **Constant**: authored literal value.
2. **Connection**: value supplied through a visible typed port.
3. **Binding**: value references a named graph/subgraph parameter or signal.
4. **Expression**: value is produced by a safe typed expression IR.
5. **Keyframed**: value is sampled from an animation curve.

Modes are mutually exclusive at the final property sink and switching modes is
undoable. Audio, LFO, expressions and keyframes all produce Signal/Value data;
operators do not contain one-off hidden audio response formulas.

The expression system is not general scripting. It supports deterministic
math, named parameters, time and documented pure functions; it cannot access
files, spawn processes, load libraries or mutate arbitrary application state.

Animation includes:

- project and component-local timelines;
- seconds, frames and musical beat units;
- keyframe interpolation, tangents and loop/hold/mirror behavior;
- reusable curves and envelopes;
- cue/state transitions and event tracks;
- parameter recording and copy/paste between compatible properties;
- deterministic scrubbing and offline evaluation.

## 8. Components, subgraphs and reuse

A component is a versioned subgraph with typed public ports, curated public
parameters, local resources and optional local time. It is not merely a visual
group.

Studio supports:

- create component from selection;
- enter/leave nested components with breadcrumbs;
- expose, hide and reorder public ports;
- expose internal parameters in named Inspector sections;
- save as a user preset;
- instantiate as an embedded snapshot;
- explicitly update a linked official component;
- detach/make local before advanced editing;
- compare version changes and run schema migration;
- expand a component transactionally for debugging.

The default graph shows semantic intent rather than implementation plumbing.
Atomic nodes remain accessible in Advanced mode and inside components.

## 9. Viewer and observability model

Only outputs with a meaningful visual or analytical representation offer a
viewer. Metadata/configuration nodes do not display fake render previews.

Viewer kinds include:

- texture/scene image;
- waveform and signal plot;
- spectrum/histogram/vectorscope;
- point/geometry viewport;
- material sphere/plane/model preview;
- event pulse/history;
- structured data table for the limited data domain.

Node viewers, the graph-corner output viewer and full-screen preview share the
same runtime and GPU resources. They are scheduled by visibility and priority,
with resolution and frame-rate budgets. Hidden or tiny viewers do not continue
full-resolution rendering.

Every operator exposes status, last evaluation time, update reason, dimensions,
format, memory estimate and localized diagnostics. Advanced profiling can show
the compiled render passes and retained state behind a semantic node.

## 10. Rendering capability scope

### 10.1 Texture and compositing

- image/video/camera/text/shape/gradient/noise generators;
- transform, crop, fit, tile, mirror and resolution operators;
- levels, curves, color-space conversion and tone mapping;
- blur, sharpen, edge, threshold, morphology and distance fields;
- displace, warp, lens, kaleidoscope and temporal distortion;
- blend modes, mask/matte, layer, switch and multi-input composition;
- feedback, delay, cache and history;
- bloom, glow, motion trail and post-processing chains;
- custom validated fragment/compute effects.

### 10.2 Signals and audio

- device/file input, waveform, spectrum, perceptual bands and loudness;
- onset, beat, tempo, transient and envelope features;
- constant, noise, oscillator, LFO, pattern and step sequence;
- math, logic, compare, map, clamp, quantize and lookup;
- filter, lag, smooth, resample, delay, hold, trigger and integrate;
- merge, split, reorder and channel selection;
- curve/keyframe output and parameter binding;
- explicit Signal/Texture/Point bridges.

### 10.3 Points, particles and simulation

- point and instance generators from grids, shapes, textures and models;
- typed attributes and attribute transforms;
- emitters, bursts, lifetime curves and deterministic seeds;
- forces, vector fields, turbulence, attractors and collisions;
- GPU simulation and indirect/instanced rendering where supported;
- Box2D-backed 2D bodies, joints, sensors and collision events;
- rope, cloth, trails, ribbons and metaball fields;
- audio and event-driven spawning/modulation;
- capability-profile fallback or publish-time rejection on mobile.

### 10.4 2D/3D scene and material

Godot is the user-confirmed primary design and source-code reference for 3D.
Follow `godot_3d_reference_plan.md`: inspect and adapt focused implementations,
retain provenance, and keep our Scene/Material/Geometry and RhythmRender
contracts. This supersedes treating 3D reference engines as equally ranked;
TouchDesigner/TiXL remain the visual-authoring workflow references.

- shared design canvas supporting landscape, portrait, square and ultra-wide;
- anchors, fit policies, safe areas and responsive layout values;
- sprites, vector/SDF shapes, text, paths, meshes and instancing;
- glTF model import, transforms, cameras and scene hierarchy;
- unlit, emissive and PBR materials;
- directional, point, spot and environment lighting;
- 2D occlusion/light composition and 3D shadow maps;
- IBL, transparency, skinning, morphs and animation in staged delivery;
- Scene2D/Scene3D to Texture outputs for ordinary composition;
- consistent linear/sRGB/HDR color management across preview and export.

## 11. Studio workflow

The default workflow is intentionally simpler than TouchDesigner:

1. choose a complete template or an empty output profile;
2. drag an asset or semantic preset into the graph;
3. obtain a valid visible result immediately;
4. edit a small curated Inspector;
5. bind a parameter to audio, time or a curve by drag/drop;
6. open internal nodes only when more control is needed;
7. validate target platforms and publish.

Required editor capabilities:

- searchable categorized palette with animated/static preview cards;
- create-and-connect from a cable using type-compatible results;
- favorites, recent items and task-oriented suggestions;
- graph minimap, bookmarks, frames/sections and alignment tools;
- full undo/redo across graph, layout, Inspector, timeline and assets;
- keyboard/IME-safe property editing;
- direct manipulation in 2D/3D preview with transactional graph writes;
- resource browser with import, relink and dependency diagnostics;
- timeline/curve editor and component-time scope;
- final output viewer within the graph workspace plus expandable views;
- generated per-node help, examples and diagnostic guidance;
- Beginner and Advanced visibility without producing different project formats.

## 12. Platform and package rules

Studio runs on Windows and macOS. Player runs on Windows, macOS, Android and
iOS. The same graph compiler and portable runtime contracts serve every target.

Each operator declares a capability profile. Publishing selects target
platforms and produces a report for unsupported texture formats, shaders,
compute requirements, precision, memory and media codecs. A semantic preset can
provide an explicit lower-cost implementation selected by profile; silent
visual substitution is forbidden.

Mobile packages contain compiled runtime programs and validated assets, never
Studio UI, graph mutation services or desktop-only code.

## 13. Extensibility boundary

Initial extensibility consists of data-driven subgraphs, presets, templates and
validated shader nodes. This delivers substantial creative freedom without
executing arbitrary native code.

A native plugin SDK is deferred until the operator ABI, permissions, signing,
crash isolation and package compatibility policy are separately approved. Core
runtime types never expose bgfx, SDL or native platform types to extensions.

## 14. Delivery stages

### A0: reference corpus and capability specification

- preserve this reference matrix and license boundary;
- define the initial operator catalog and output profiles;
- define representative benchmark compositions;
- approve the parameter, time, component and preset models.

### A1: typed graph and evaluator foundation

- implement value types, operator descriptors and package registries;
- implement demand roots, dirty propagation, state slots and diagnostics;
- implement component/subgraph persistence and expansion/compilation;
- implement Null-backend deterministic tests.

### A2: Texture + Signal vertical slice

- deliver core generators, filters, compositors and explicit feedback;
- deliver audio features, LFO/math/map/smooth and parameter binding;
- deliver node/final viewers with GPU resource sharing;
- build several complete horizontal, vertical and square compositions.

### A3: time, presets and easy authoring

- implement parameter modes, curves, timeline and component-local time;
- implement the preset/package registry and Beginner workflow;
- deliver official presets/templates with generated documentation;
- validate deterministic offline stepping.

### A4: 2D, particles, physics and lighting

- migrate audited old capabilities into the new operator contracts;
- add GPU point/particle flows and explicit audio/event inputs;
- validate state retention during compatible live edits;
- publish desktop and mobile capability profiles.

### A5: 3D and material system

- glTF, camera, material, light and Render3D-to-Texture slice;
- PBR, IBL, shadows, instancing and color-management validation;
- animation/skinning/morph stages after the static pipeline is accepted.

### A6: advanced compositing and product hardening

- safe custom shader workflow, compute and render-pass diagnostics;
- performance budgets for large graphs and multiple viewers;
- reference-composition visual tests and export parity;
- Windows product acceptance, then Android Player, then the final Apple port
  (macOS Studio/Player and iOS Player), by the user's 2026-09-07 decision.

## 15. Capability acceptance

Cluster playback extends runtime inputs, not the graph editing model. The
planned SessionTime, SharedAudioFeatures, ParticipantRole, CueEvent and bounded
ParticipantValue interfaces are described in `cluster_playback_plan.md`.
They publish typed snapshots, offer explicit offline/simulated-role behavior,
and reuse the same audio analysis and content registry. Beginner templates
provide group color/portrait/audio-reactive experiences without exposing
socket or handshake plumbing as required user nodes. Cluster work does not
introduce concurrent remote graph editing or bypass package capability checks.

The system is not called TouchDesigner/TiXL-class merely because nodes can be
connected. Before that claim, it must demonstrate:

- at least one non-trivial composition combining Texture, Signal, Point and
  Scene domains without a bespoke fixed-effect runtime path;
- a reusable semantic component whose public Inspector controls internal nodes;
- constant, connection, binding, expression and keyframe control of the same
  compatible parameter;
- local component time, deterministic scrubbing and offline export;
- feedback, multi-pass composition and intermediate viewers without CPU image
  readback;
- a 3D scene rendered to texture and composed with 2D/audio-driven effects;
- profile validation and playback of the same package on every supported
  Player platform targeted by that package;
- creation of a polished result from a template by replacing assets and
  adjusting fewer than ten exposed controls;
- user presets and detached editable copies that do not break when the official
  catalog is updated.
