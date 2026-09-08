# Independent 3D scale and Harmonic City

`scene.transform` now has optional `scale_x`, `scale_y`, `scale_z` inputs and
properties. Axis factors default to one and multiply uniform scale. Each factor
and final product is clamped to `[0.001, 100]`, retaining the invertible minimum
of a single transform. Negative scale/mirroring is not introduced. Scale precedes
X/Y/Z rotation and translation; existing GLM matrix and inverse-transpose adapters
perform the math. Nested transforms retain existing affine and scene budgets.

Inputs append after the original eight slots. The program reader pads omitted
optional slots, so old programs and absent axis properties retain uniform scale.
This is old-package compatibility in the updated reader, not a promise that old
players accept newly authored ports.

## Composition

“律动之城 / Harmonic City” has 290 reachable nodes and 471 edges. Thirty-two
real audio bands drive 64 columns; half-height translation anchors each column
on the floor. Four shared pillar materials, a floor and 18 grid lines form
83 instances using one cube. Balanced merges avoid the quadratic intermediate
copies of a sequential chain. Two lights, slow scene rotation, camera and existing
blur/composite passes complete the image. No shader, budget or dependency is added.

All nodes remain editable. Audio maps are arranged in four groups of eight;
`input_max` controls sensitivity, `output_max` height, and the shared emission map
controls highlights. Silence lowers the columns. Rotation is explicitly driven
by time and does not pretend to be a beat detector.

The first render had short, dim columns. Inspection led to higher sensitivity,
closer framing, brighter emission and visible grid lines. The revised capture is
`out/windows-release/harmonic-city-music-captures/resonance_demo.png`.
The actual D3D thumbnail records synthetic input and package hash separately.
This is an authored Advanced candidate, not a user-accepted quota increment.
Reuse is recorded in `provenance/harmonic_city.json`; no third-party code is added.

## Verification

- `out/axis-contract-tests.log`: graph/runtime/program checks pass: transform
  order, inverse-transpose normals, wired precedence, shared mesh upload, minimum
  products and a legacy eight-slot program evaluated with its original scale.
- `out/axis-android-contract-tests.log`: the same runtime/program checks pass on
  USB Android `e2b3b128`.
- `out/harmonic-city-review-tests.log`: actual Studio bind/save/publish/clear/reopen,
  four decoded-input D3D comparisons and scene GPU tests pass. All 256 pixels of
  the axis fixture verify property and connected-input bounds after serialization.
- `out/axis-gles-tests.log`: the same exact bounds and previous GLES scene,
  texture, blending and device recreation tests pass on the phone.
- `out/harmonic-city-final-tests.log`: seven source/catalog/scene/program/music
  fixture/export checks pass. The Studio button creates a 480-frame, 16-second
  H.264 MP4. During concurrent soak/build activity, parent p50/p95 frame time is
  19.8827 / 31.8989 ms; this is not an isolated performance baseline.

D3D mean RGB differences (8-bit channel units): music/silence 6.5494,
low/silence 18.0803, high/silence 12.3361, low/high 13.6014.

`out/harmonic-city-gles-tests.log`: actual Studio-published 1,785,542-byte package,
960 music and 960 silent frames at 640×360. Music p50/p95 **4.55786 / 5.11318 ms**,
silence **4.48661 / 5.01078 ms**, stable textures **6,220,804 bytes**. At
2/6/10/14 seconds, RGB differences are **7.03668 / 5.96321 / 6.49952 / 5.93995**;
music RMS reaches 0.236431, silent RMS zero. These native offscreen measurements
include `glFinish`; they do not establish APK/display or sustained refresh behavior.

Windows Studio/Player deploy directories contain executables, resources and 20
DLLs. Android APK and matching relink materials rebuild; the embedded example
remains Resonance Gate, with Harmonic City available as an external package.
USB installation policy still blocks application acceptance. No APK lifecycle
result is inferred from native tests.

Subsequent canvas work and editor/export remeasurements are recorded in
[canvas frame work](canvas_frame_work_2026-09-08.md); export UI frame cost remains
measurable after the snapshot-copy fix.
