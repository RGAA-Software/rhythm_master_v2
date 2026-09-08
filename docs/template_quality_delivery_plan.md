# Template quality and delivery plan

## Confirmed target and current evidence

[Product scope](product_scope.md) governs this music visualization content library.
Templates are editable examples and reusable starting points within a
TiXL/TouchDesigner-class authoring product. Their quantity and visual quality do
not replace acceptance of original graph creation, live node previews, parameter
control and component reuse. Music-driven templates must expose usable music
controls and demonstrate audible-input changes through visual response. Reusable static backgrounds and other building
blocks may support a composition, but do not independently establish a complete
music visualization template. Actual music playback acceptance is distinct from
synthetic feature tests and silent screenshots.

Latest user instruction: at least **50 Basic templates and 50 Advanced templates**.
This replaces the earlier 24-template minimum. The independent requirements of
40 semantic nodes and 120 visually distinct presets remain. Counts measure
distinct accepted compositions, not recolors, aspect ratios or device variants.

The inventory currently contains 32 runnable examples, 11 semantic components
and 128 preset records (106 native plus 22 semantic). The user rejected the earlier examples' visual quality. No accepted
Basic/Advanced allocation has been established. Functional test results remain
valid for the behavior they cover; they do not establish visual acceptance.

## What to do next

2026-09-08 execution update: the original starting sequence below is historical;
blur, spatial noise, displacement, reference scenes and media integration already
have working implementations. Follow the [current rendering/authoring roadmap](rendering_capability_roadmap.md)
for the next work. Produce editable music-driven content with each feature batch,
retain the 50 Basic + 50 Advanced quality target, and defer long-duration testing
to final integrated acceptance. Android overlay installation is now successful.

1. Build the reusable effects needed for richer compositions. Start with bounded
   GPU blur and a composable glow stack, then spatial texture noise, displacement,
   kaleidoscope and controllable temporal feedback. Current signal noise is not a
   procedural image generator. Existing feedback is a foundation, not a completed
   motion-trail effect. Avoid adding unrelated infrastructure during this work.
2. Deliver three Basic and three Advanced quality-reference templates. Use the
   existing live audio input and deterministic test signals so pending file-media
   integration does not block independent visual work. A layered audio/neon
   composition is the first end-to-end delivery: effects, editable graph, useful
   controls, actual motion preview and standalone Player package together.
3. Complete the content browsing workflow alongside that batch: Basic/Advanced
   and subject filters, localized descriptions, actual thumbnails, short previews,
   exposed controls and explicit platform profiles. Extend categorization to the
   separate semantic library. The newly grouped native-node palette is only the
   initial editor correction.
4. Review the six references for composition, motion, editability and measured
   performance. Fix shortcomings before producing larger batches. Record visual
   review separately from automated tests; do not claim user acceptance until it
   is actually given. Continue independent work without asking for permission at
   each implementation step.
5. Produce reviewed batches toward 50 Basic and 50 Advanced. Expand the reusable
   semantic library and presets from useful effects rather than cloned examples.
   Image/video, full timeline, particle fields and richer 3D capabilities feed
   the relevant batches as their shared runtime modules become ready.
6. Complete the remaining Windows product and Android local Player gates from
   steps 1–7. Validate declared mobile variants on the actual device; a Windows
   effect does not automatically acquire Android support. Apple remains the last
   platform stage and communication remains the final overall stage.

## First reference batch (in progress; acceptance pending)

| Tier | Composition | Purpose and key work |
| --- | --- | --- |
| Basic | Layered neon audio ring | Clear hierarchy, glow, distinct band response, restrained idle motion |
| Basic | Flowing atmospheric background | Spatial noise, displacement, deliberate palette and looping motion |
| Basic | Spark and dust scene | Foreground/background depth, coherent emission, reusable glow controls |
| Advanced | Audio-reactive kaleidoscope tunnel | Layered procedural imagery, repeat/warp, rhythm response and camera-like motion |
| Advanced | Flow-field particle trails | Directed fields, temporal accumulation, controlled highlights and detailed movement |
| Advanced | Lit audio sculpture | Composed geometry, materials, deliberate lights/camera and audio modulation |

Layered neon and atmospheric clouds now have runnable Basic candidates. Prismatic
lotus uses polar depth, independently evolving mirrored fields and antialiased
contours. Firefly garden, Stellar currents and Orbital reliquary now complete the
six authored candidates, with Android Release measurements and real browser
previews. See `validation/reference_batch_2026-09-07.md` for evidence and remaining
visual/product acceptance. These are still candidates, not user-accepted counts.

These are scope proposals for concrete production, not assertions that all
required operators exist. Prefer a well-composed modest scene over simulating
complexity through excessive counts or render cost. Basic content must still be
visually finished. Advanced content must offer a materially richer composition.

## Reuse entry points and compatibility checks

Existing local reference inventories identify TiXL revision
`fbc994d923e8a0142d2ff1b772e4d12248c5b0ba` and Material Maker revision
`ad19fcf0ee34a7caf74df709dc4de7112f0d467d`. Located candidate files include:

- TiXL `Operators/Lib/Assets/shaders/dx11/image-quad-kaleidoscope.hlsl`,
  `Operators/Lib/Assets/shaders/img/generate/PerlinNoise2d.hlsl` and
  `Operators/Lib/Assets/shaders/img/compute-Displace.hlsl`.
- Material Maker `addons/material_maker/nodes/gaussian_blur_x.mmg`,
  `gaussian_blur_y.mmg`, `directional_warp.mmg` and `noise.mmg`.

These paths are a reuse shortlist, not completed file/license audits or imports.
Inspect selected implementations and their included code, retain exact notices
and record modifications before adoption. Translate only the focused algorithms
and shader portions needed for RhythmRender's contracts. Validate uniforms,
samplers, alpha/color handling, bounded intermediate textures and D3D11/GLES
compilation; HLSL compute and Material Maker's generator context cannot simply be
dropped into the current fragment-pass adapter. Use existing first-party passes
and graph services for composition. Dependencies and tools still prefer vcpkg.

## Acceptance and tracking

For each tier, track authored / functional checks passed / visual review passed /
target-device validated separately. Every accepted template needs:

- A distinct composition and useful defaults, documented common controls and
  complete Chinese/English metadata.
- A reproducible project, package, deterministic reference inputs and exact
  provenance/licenses for reused content.
- Actual rendered thumbnails and a motion preview; visual inspection of spatial
  hierarchy, temporal development, parameter extremes and idle/silent behavior.
- Save/reopen and publish/Player agreement, bounded allocations and target-profile
  checks, plus measured frame time and memory on the declared device/resolution.
- Clear platform eligibility. Mobile quality variants retain the same template
  identity and require actual device evidence before being advertised as validated.

Exact performance budgets follow the existing platform profiles and measured
reference batch; do not invent frame-rate guarantees ahead of measurement.

The pending Windows FFmpeg application-license choice and Android USB installation
prerequisites remain tracked separately. Neither justifies resuming communication
or counting unfinished media/device work as complete.
