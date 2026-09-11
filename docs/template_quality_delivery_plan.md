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

2026-09-10 content direction supersedes the older Basic-template target below:
the catalog keeps only high-end, editable Advanced works. Removed simple examples
and their obsolete review records do not count toward any target. The active
content target is **50 distinct accepted Advanced templates**, backed by reusable
components and presets; historical Basic/Advanced wording records earlier plans.

Historical user instruction required **50 Basic templates and 50 Advanced templates**
and had replaced the earlier 24-template minimum. It is retained here as history;
the 2026-09-10 Advanced-only target above now governs execution. The independent
requirements of 40 semantic nodes and 120 visually distinct presets remain. Counts
measure distinct accepted compositions, not recolors, aspect ratios or device variants.

The 2026-09-09 inventory contains 45 runnable examples,
29 semantic components and 191 preset records (133 native plus 58 semantic). The user rejected the earlier examples' visual quality. No accepted
Basic/Advanced allocation has been established. Functional test results remain
valid for the behavior they cover; they do not establish visual acceptance.

## What to do next

2026-09-09: follow [the overall implementation plan, P7](master_implementation_plan.md)
for the current production batches, 10 content families and separate authored,
functional, visual-review and device evidence. Its next implementation queue
replaces the historical sequence below; the active goals are 40 reusable components,
120 visually independent presets and 50 accepted Advanced templates.

2026-09-08 execution update: the original starting sequence below is historical;
blur, spatial noise, displacement, reference scenes and media integration already
have working implementations. Follow the [current rendering/authoring roadmap](rendering_capability_roadmap.md)
for the next work. Produce editable music-driven content with each feature batch,
retain the 50 accepted Advanced quality target, and defer long-duration testing
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
5. Produce reviewed batches toward 50 accepted Advanced templates. Expand the reusable
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

Artifact licensing and redistribution materials remain separately tracked; this
plan does not select the project's outbound license. Android USB overlay installation
now works; broader device and lifecycle evidence remains separately tracked.
Neither justifies resuming communication or counting unfinished work as complete.

## P7 review ledger (2026-09-10)

`content_quality_index.json` in docs is the generated current inventory;
`content_reviews.json` is the append-only per-source review history. Run
`python tools/audit-content-quality.py` to regenerate and add `--check` to detect
stale inventory. Source changes invalidate the applicable review without erasing
history. This is independent of runtime manifest format and does not migrate packages.

The current inventory is 53 templates, 29 components and 206 preset records.
117 presets are default-value records and excluded from the visual preset target;
the remaining 89 are candidates, not automatically independent accepted presets.
The initial ledger records zero accepted quality items: earlier functional evidence
remains valid within its recorded scope, but has not been converted into a completed
four-stage source-bound quality review. This is not a claim that all existing content
is bad or that earlier user feedback has been revoked.

Each review includes exact source SHA256, reviewer/date, functional, music, visual
and device decisions, concrete evidence files and notes, and an independence group.
All four stages must pass for the quality count; pending/failed/not-applicable stages
do not silently bypass this gate. Default presets, duplicate independent-work groups,
identical preset settings and missing evidence cannot inflate accepted totals.
User acceptance is separate and is never inferred from a tool pass or agent review.
`tools/test-content-quality.py` checks stale reviews, pending stages, missing evidence,
default exclusion and duplicate-count rejection.

First 2 Basic + 2 Advanced calibration batch: Ink Tide, Porcelain Pendulum,
Chromatic Loom and Resonant Arcade. These existing editable music works provide
organic texture, a light mechanical composition, woven geometry and architectural
space. Recheck current source/package identities, real PCM contrasts and motion,
then record shortcomings and revisions before expanding the production batch.
This selection is a review queue, not a quality acceptance statement.

The first four works now pass the music stage against exact current source hashes:
seven decoded PCM inputs (including 700 Hz mids and 0.25x/1.25x music levels),
ten D3D comparisons and four packaged-arrangement GLES checkpoints. The extended
check found isolated FFT-bin coverage gaps in all four works; each now composes
contiguous low/mid/high peaks from existing operators. See
[calibration evidence](validation/calibration_band_coverage_2026-09-10.md) and
[Porcelain evidence](validation/porcelain_frequency_coverage_2026-09-10.md).
Public-control extremes, resource replacement, declared-profile acceptance and
final visual review remain pending; the music-stage pass does not add an accepted
template or infer user acceptance.
