# Four reusable image-processing chains — 2026-09-09

These entries extend [input-processing components](input_components_2026-09-09.md).
They accept the author's texture and retain an editable internal graph. Each has
six controls and two presets; preview stripes stay outside the inserted component.
Extracting the common catalog writer initially kept Flow glass files byte-identical.
A subsequent usability correction bounds public response to 0–2, flow to ±0.5,
and pace to ±45 (±0.25 for contour phase), including Flow glass. Catalog content
version is 0.1.1. Existing edited/older definitions retain the normal explicit
conflict behavior; they are not silently overwritten. Default runtime packages
remain byte-identical after these authoring-only bounds, so the pixel evidence
still describes the final packages. No new rendering dependency is added.

| Component | Internal nodes / preview instructions | Purpose and variant |
| --- | --- | --- |
| Prism fold / 棱镜折叠 | 11 / 14 | Seven rotating mirrored sectors, two-band steering, saturation/glow; variant changes to three sectors, framing and reverse motion |
| Contour engraving / 轮廓刻线 | 11 / 14 | Luminance slices with independently editable count, width, contrast and glow; variant uses broader, fewer slices |
| Motion echo / 运动回声 | 10 / 13 | Source rotation/scale and fading, zooming, turning history; variant reverses motion and tightens the history spiral |
| Soft glow / 柔光合成 | 9 / 12 | Narrow/wide blur composite with bass/high energy; variant broadens the halo and changes source exposure |

Soft glow preserves source motion and has no independent animation clock. It is
a dual-radius glow composition, not a new physically based bloom implementation.
Motion echo has state: arbitrary analytic seeking cannot reconstruct its history;
restart and simulate when a deterministic history is required.

## Real decoded music

At the same four-second scene time and 1280×720 capture size, all four required
RGB mean differences exceed the unchanged 0.15 threshold:

| Component | Music/silence | Low/silence | High/silence | Low/high | Stable accounted texture bytes |
| --- | ---: | ---: | ---: | ---: | ---: |
| Prism fold | 31.6097342 | 29.4781131 | 30.2078892 | 29.6959986 | 23,961,604 |
| Contour engraving | 6.3925926 | 24.3583333 | 18.3777778 | 18.3592593 | 25,804,804 |
| Motion echo | 12.7689377 | 19.2248206 | 24.9615954 | 31.9127282 | 29,491,204 |
| Soft glow | 2.7199074 | 15.1111111 | 7.3648148 | 7.7462963 | 28,886,404 |

The test recognizes existing mapping/contour/trail/blur processors with at least
two reachable audio bands, in addition to the previously covered displacement and
3D paths. Exact expected instruction counts and the four PCM/pixel comparisons
remain mandatory. Captures are actual GPU results, not hand-authored thumbnails.

## Android and caching

All four defaults and all four variants pass native USB Adreno 650 GLES checks,
60 frames each at the 640×360 scene extent, including nonblack final output,
valid handles/budgets, multipass correctness and device recreation.

Soft glow exposed a test assumption: it reused its static offscreen textures and
had only the final presentation pass on frame 60. The test formerly required
multiple passes specifically on that frame. It now verifies that actual
composition occurred during the run, keeps the final pixel/orientation checks,
and additionally rejects invalid outputs or budget fallback on every frame.
Soft glow has **13 peak passes / 1 final pass** for default and variant. This is
expected caching, not a rendering failure; the product renderer was unchanged.

Variants are inserted into the test's existing gradient project before package
validation, exercising caller input rather than only the preview harness.
These native checks are functional evidence, not sustained mobile frame-rate
acceptance. Official component entries are not added as duplicate complete works
to Android's built-in effect list.

## Records

`tools/input_component_recipes.py` contains the first-party compositions;
`tools/author-input-components.py` writes the bounded harnesses and interfaces.
Source hashes and existing adapter references: `provenance/input_components.json`.

Logs: `out/r6-processing-batch-build.log`, `out/r6-processing-batch-contracts.log`,
`out/r6-{component}-music.log`, `out/r6-{component}-{default|variant}-android.log`,
`out/r6-processing-android-build.log`, `out/r6-processing-thumbnails.log`,
`out/r6-processing-deploy-build.log`. Each entry retains exact package/pixel
thumbnail hashes. All 22 entries pass the catalog insertion/history/package tests.
All four new entries also pass actual Chinese and English browser selection,
GPU preview, fixed size, explicit insertion/undo and resource release checks.
Captures/logs: `out/r6-processing-browser/{component}/{locale}.*`.
The Chinese Prism fold title and connection guidance were visually inspected.

Inventory becomes **22 semantic components / 44 semantic + 132 native preset
records / 42 complete-project examples**. These processors are not four new
complete works or user-accepted visual presets. The 40-component, 120-independent
preset and 50 Basic + 50 Advanced quality targets remain in progress.
