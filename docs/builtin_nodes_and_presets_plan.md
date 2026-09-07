# Built-in nodes, presets and templates plan

> Status: incomplete. Inventory: eight semantic components, 111 preset records and
> 25 runnable example projects, including two Basic candidates and one Advanced candidate. These are not
> visually accepted product counts.
> User review rejects current template quality; polished-template acceptance remains open.
>
> Date: 2026-09-06

## 1. Product rule

Rhythm Master must be easier to start than a general visual programming tool.
An empty graph and hundreds of atomic operators are not an acceptable default
experience. Users should be able to choose a template, replace media, change
colors and bind audio without understanding render passes or FFT processing.

Ease of use is built as four layers:

```text
runtime operators -> semantic nodes -> presets -> complete templates
```

The layers share one graph/runtime format. Beginner mode hides implementation
detail; it does not use a separate simplified engine.

## 2. Four content layers

### 2.1 Runtime operators

Runtime operators are stable, typed and narrowly responsible. Examples include
Blur, Composite, Spectrum, Smooth, Particle Emit, Point Force, Render Scene and
Tone Map. They provide the maximum composition freedom and are visible in
Advanced mode.

Adding an artistic variation normally does not add a new C++ operator. A new
operator is justified only by new execution semantics, data conversion or a
meaningful optimized implementation.

### 2.2 Semantic nodes

A semantic node is a maintained component/subgraph representing an artistic
intent such as Audio Reactive Ring, Neon Particle Field or Lit Cloth. Its
Inspector exposes a curated set of internal parameters in named sections.

Semantic nodes:

- produce a useful output immediately;
- expose common controls and hide implementation plumbing;
- allow public ports to be revealed on demand;
- can be entered for advanced editing;
- can be detached into a local editable copy;
- can declare low/medium/high quality implementations;
- preserve stable public IDs across compatible updates.

### 2.3 Presets

A preset is a named, versioned parameter/subgraph configuration for an operator
or semantic node. Presets provide meaningful visual variants without creating
duplicate runtime types.

Every artist-facing node has a tested `Default` preset. Default means visually
useful, numerically safe and representative; it does not mean zero-initialized.

### 2.4 Complete templates

A template is a complete project with output profile, graph, timeline, assets,
audio mappings, documented editable controls and preview media. Templates are
the primary first-run entry point.

Templates cover landscape, portrait and square canvases and declare compatible
Player targets. They are test data/content packages, not scene-construction
code embedded in the application executable.

## 3. Content storage and ownership

Official content is stored as independently versioned packages:

```text
content/
  operators/<domain>/...
  semantic/<category>/<stable-id>/...
  presets/<category>/<stable-id>/...
  templates/<category>/<stable-id>/...
  assets/sha256/...
  localization/<locale>/...
```

Application code registers generic operator factories and content-package
loaders. It does not contain special branches that construct demo scenes. Test
and content-authoring tools generate and validate official examples into their
package directories.

Official, user and future marketplace content use separate namespaces. A
project records the resolved content ID, version, hashes and an embedded or
package-resolvable snapshot needed for reproducible playback.

## 4. Required metadata

Each operator, semantic node, preset and template includes appropriate fields
from this model:

- stable content ID, namespace, semantic version and schema version;
- localized title, summary, keywords and task-oriented help IDs;
- category, tags, difficulty and maturity;
- author, license, source and redistributable-asset records;
- compatible input/output types;
- public parameters with defaults, bounds, units, widgets and descriptions;
- target-platform and runtime capability requirements;
- quality tier, estimated GPU/CPU/memory cost and preferred preview budget;
- deterministic seed and expected initial-time behavior;
- dependency IDs and compatible version ranges;
- static thumbnail, optional short preview clip and reference-frame hash;
- migration rule or explicit incompatibility declaration.

Translated text is never a content ID. All official content ships complete
Simplified Chinese and English metadata under the localization rules.

## 5. Initial operator catalog target

The counts below are product targets for the first complete desktop Studio, not
a reason to add shallow duplicates. One operator may cover a coherent family
through typed modes when those modes share execution semantics.

| Domain | Minimum stable operators | Required coverage |
| --- | ---: | --- |
| Value/utility | 15 | constants, vectors/colors, math, logic, compare, convert, select |
| Time/signal | 20 | time, LFO, noise, curves, map, filter, delay, trigger, sequence |
| Audio | 15 | input, waveform, spectrum, bands, loudness, onset, beat, envelopes |
| Texture generators | 15 | image/video/text, shapes, gradients, SDF and procedural noise |
| Texture filters | 25 | transform, color, blur, edge, morphology, warp and stylization |
| Composite/history | 12 | blend, layer, mask, switch, render target, cache and feedback |
| Point/particle | 20 | generators, attributes, emitters, forces, fields and renderers |
| 2D scene/simulation | 15 | sprites, paths, lights, occluders, bodies, joints, rope and cloth |
| 3D/material | 25 | geometry/model, camera, lights, PBR/unlit, scene render and post |
| Output/layout | 8 | canvas/profile, safe area, display, wallpaper, capture and publish |

These targets intentionally overlap at interfaces but not as duplicate fixed
effects. Catalog reviews measure coverage, composability, documentation and
visual quality, not only count.

## 6. Initial semantic and preset catalog

The first complete Studio targets:

- at least 40 polished semantic nodes;
- at least 120 visually distinct official presets;
- at least 50 complete Basic templates and 50 complete Advanced templates
  (100 distinct accepted templates in total; supersedes the former 24-template
  target by the user's latest instruction);
- at least one tutorial/template for every major runtime domain;
- at least six portrait-first and four square-first templates;
- mobile-compatible variants for the templates marketed for mobile Player.

Basic and Advanced describe authoring complexity and visual scope, not paid
editions or automatic hardware requirements. Both tiers require polished output.
Basic templates focus on a clear task, approachable controls and an understandable
graph. Advanced templates deliver richer spatial/temporal composition through
multiple interacting systems, with curated controls and explicit performance
profiles. A complex graph or a larger particle count alone does not qualify a
template as Advanced.

Each template has one stable identity and one catalog tier. Recolors, parameter
presets, aspect-ratio adaptations and device-quality variants do not count as
additional templates or count toward both tiers. Track authored, functionally
validated and visually accepted entries separately for Basic and Advanced.
The current 22 examples require review and rework before counting toward either
50-template acceptance target; their presence does not establish tier acceptance.

Immediate production sequence and the first quality-reference batch are recorded
in `template_quality_delivery_plan.md`. The 40-semantic-node and 120-preset targets
remain independent requirements.

### 6.1 Audio visualization

- linear, mirrored, radial, polygon and path spectra;
- waveform ribbons, filled waveforms and oscilloscope styles;
- album-art rings, equalizers and typographic visualizers;
- beat pulse, transient flash, bass deformation and multi-band color response;
- classical/acoustic-sensitive defaults as well as electronic/DJ defaults.

### 6.2 Particles and simulation

- sparks, dust, snow, rain, embers, fireflies and star fields;
- radial bursts, trails, flock/flow and texture-shaped emission;
- Box2D collisions, bouncing shapes, joint chains and sensor events;
- rope/cloth audio response, metaballs and fluid-like fields;
- emissive, lit and shadow-casting variants with bounded quality tiers.

### 6.3 Texture and motion graphics

- gradient/noise backgrounds, kaleidoscope, tunnel and feedback looks;
- glitch, chromatic, pixel, scanline, VHS and displacement styles;
- bloom, light streak, echo, temporal accumulation and motion trails;
- image parallax, collage, slideshow and video-reactive treatments;
- masks, mattes, transitions and reusable post-effect stacks.

### 6.4 2D/3D scene

- neon shapes, logo reveal, text title and clock layouts;
- responsive landscape/portrait compositions;
- 3D model pedestal, orbit camera and environment-light templates;
- instanced geometry, point clouds and audio-deformed meshes;
- PBR, hologram, wireframe, emissive and toon-style materials.

## 7. Easy authoring workflow

### 7.1 Project start

The start screen offers task-oriented choices rather than only an empty graph:

- Audio Visualizer;
- Image/Video Motion Design;
- Particle Scene;
- 2D Physics/Lighting;
- 3D Model Scene;
- Landscape, Portrait or Square Blank Project.

Each choice opens filtered templates and explains target-platform cost.

### 7.2 Palette and search

The palette supports:

- categories, tags, fuzzy search and localized aliases;
- thumbnail/short preview, description, input/output types and cost badge;
- favorites, recent items and project-compatible suggestions;
- Beginner view showing semantic nodes/presets first;
- Advanced view exposing runtime operators;
- opening from a cable with incompatible items filtered out;
- drag/drop with automatic compatible connection;
- warnings before inserting content unsupported by selected Player targets.

Preview cards use cached media. Opening a palette with hundreds of items must
not instantiate and run hundreds of graphs.

### 7.3 Inspector

Semantic-node controls are grouped by intent, for example Audio, Shape,
Appearance, Motion and Advanced. Controls expose units, safe ranges, reset,
animation/binding status and short help. Advanced internal parameters are
collapsed until requested.

Users can:

- apply and compare presets without losing undo history;
- randomize only properties that opt into safe randomization;
- lock colors, seeds or parameter groups while exploring variations;
- drag a Signal/Event output onto a compatible property to create a binding;
- promote an internal parameter to the component interface;
- save the current configuration as a user preset.

### 7.4 Learn by modification

Official templates identify a small `Start Here` control set and provide
localized inline steps. Users can progressively reveal the internal graph,
inspect node viewers and open generated documentation. A template remains a
normal editable project; tutorial mechanics do not create a separate format.

## 8. Versioning behavior

Official content updates never silently change a saved visual result.

- inserting a preset records its resolved values;
- inserting a semantic component records its stable public definition and
  compatible implementation reference/snapshot;
- compatible updates are shown as explicit actions with a change summary;
- breaking updates use a new major content version;
- detaching creates an entirely project-owned local component;
- missing official packages fall back to the stored project snapshot when
  allowed by the package policy;
- Player consumes compiled operators/assets, not a mutable online catalog.

User presets are stored under application data and can be exported/imported as
content packages. Cloud synchronization and marketplace distribution are later
services over the same package model, not prerequisites for local use.

## 9. Cross-platform content profiles

Content metadata declares one or more profiles:

- Desktop Standard;
- Desktop High;
- Mobile Standard;
- Mobile High where supported.

A profile constrains texture sizes/formats, render-target count, compute use,
particle/point count, shader features, model complexity, lights/shadows, memory
and expected frame budget. Presets may include explicit profile variants that
preserve the artistic intent. Studio previews the selected profile and reports
differences before publishing.

There is no hidden automatic downgrade that changes a published effect without
diagnostics. Unsupported content fails validation for that target.

## 10. Content production pipeline

Official content follows the same review process as code:

1. author using public operators and assets;
2. expose a small, coherent public parameter surface;
3. define defaults, ranges, seeds and quality profiles;
4. add Simplified Chinese and English metadata/help;
5. record asset/source licenses;
6. generate thumbnail, short preview and deterministic reference frames;
7. run schema, dependency and target-capability validation;
8. run Null-runtime and real-GPU tests;
9. visually review at representative audio inputs and design resolutions;
10. publish a signed content package.

Content tests include silence, classical/acoustic material, speech and
electronic music so presets are not tuned only for high-energy bass tracks.

## 11. Automated acceptance

Every official node/preset/template must pass relevant checks:

- schema and dependency validation;
- load/save and content-version migration;
- deterministic first/reference frames where applicable;
- finite values and bounded allocations under declared parameter ranges;
- missing-resource and unsupported-profile diagnostics;
- no routine GPU readback for visual previews;
- no hidden hard-coded asset path;
- no custom C++ demo construction path;
- complete primary-locale metadata;
- desktop render test and selected mobile profile compile test;
- package license manifest completeness.

Catalog-level gates verify unique IDs, no cyclic package dependencies, search
coverage, duplicate-appearance review and startup/palette performance.

## 12. Delivery order

1. define catalog schema and authoring package format;
2. implement generic registry, search and preview metadata;
3. create core Texture + Signal operators and ten semantic vertical slices;
4. build the first eight complete templates and validate the easy workflow;
5. add time/curve/binding presets;
6. migrate and redesign useful 2D/audio/particle capabilities;
7. add 3D/material content after its runtime slice is accepted;
8. reach catalog targets through reviewed content batches;
9. validate Windows first, then Android Player, then macOS Studio/Player and iOS
   Player in the final Apple port (user decision, 2026-09-07).

No phase is complete because a placeholder node is registered. A node counts
only after it renders/evaluates correctly, is documented, has a useful default,
survives persistence and passes its target-platform tests.
