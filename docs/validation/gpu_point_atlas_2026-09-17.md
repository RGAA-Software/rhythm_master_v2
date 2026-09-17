# GPU point sprite atlas validation — 2026-09-17

W1.4 first increment: textured sprite atlas and per-particle shape variation for
GPU points, keeping the fixed four-vec4 simulation contract unchanged.

The third shape component of each particle record now carries a stable random in
[0,1) assigned at spawn (an appended LCG draw; all existing spawn values are
bit-identical). The simulation copies it unchanged every step and `gpu.map`
already preserved reserved fields. `GpuPointAtlas` (project texture handle plus
1–64 columns/rows) extends `GpuPointStyle`; the render vertex stage selects one
stable cell per particle from that random, and the fragment stage modulates the
existing analytic soft envelope with the sampled cell shape and tint. Without an
atlas the draw is pixel-identical to the previous analytic sprite. References:
bgfx sprite-sheet selection and Godot particle texture variation, recorded in
`provenance/gpu_point_atlas.json`. The `gpu.render` node gains an optional
`atlas` texture input (appended port, old graphs unchanged) and
`atlas_columns`/`atlas_rows` parameters with en-US/zh-CN labels.

Contract tests reject out-of-range grids, depth/self-target or foreign atlas
textures on the Null backend (`out/w14-atlas-unit.log.runs/`). The D3D11 probe
(`out/w14-atlas-probe.log.runs/1789609072892383400.log`) asserts a hollow ring
cell darkens the sprite center while the ring stays bright, that texture v=0 is
the sprite canvas-top via an asymmetric quadrant cell, and that a two-cell
solid/empty atlas retires roughly half of an identical 128-particle crowd
(lit-pixel ratio inside [1/4, 3/4] of the single-cell baseline). One retained
intermediate failure (`out/w14-atlas-debug.log.runs/`): the first ring probe
sampled the sprite's faded edge because the spawn size random narrows the quad;
the assertion now samples a radius that is inside every possible quad.

Graph/runtime regressions `gpu_particle_graph`, `gpu_sampling`, `gpu_particles`,
`work_library`, `event_gpu` and `gpu_point_contracts` pass
(`out/w14-atlas-regression.log.runs/1789609206154391700.log`). Windows SM5 and
Android GLES 3.0 shader compilation pass (`out/w14-shader-check/`). Android
device revalidation stays in the final platform phase.

Remaining W1.4 work: soft depth intersection (Godot proximity fade reference)
and layered volume presentation in an authored work.
