# GPU point soft depth intersection validation — 2026-09-17

W1.4 second increment: soft depth intersection for GPU points with Godot 4.5.1
proximity-fade semantics (`provenance/gpu_point_soft_depth.json`).

`GpuPointSoftDepth` carries the scene depth attachment, the near/far/orthographic
projection that wrote it and a view-space fade distance. The point vertex stage
forwards canvas-space fragment position and the particle depth (spawn center z
plus accumulated velocity z, both inside the fixed record); the fragment stage
reconstructs the scene distance with the same linearization as
`depth_of_field_weight.sc` and multiplies alpha by
`smoothstep(0, distance, scene_z - particle_z)` — the well-defined form of
Godot's reversed-edge expression. No depth buffer is written by the 2D point
pass and no invert handling is needed because the canvas sampling convention
matches `GpuPointSampling`. The graph exposes this as an optional `depth` port
on `gpu.render` (appended after `atlas`, old graphs unchanged) plus a
`soft_distance` parameter, and `gpu.particles` gains `center_z` so emitters can
place their layer in view units; the runtime previously hardcoded z to 0.
Center z validation moved from the canvas [-4,4] band to view-space
[-10000,10000]; x/y validation is unchanged.

Contract tests reject non-positive near/distance, color textures, extent
mismatch and foreign handles on the Null backend (`out/w14-soft-unit.log.runs/`).
The D3D11 probe renders an orthographic plane at distance 6 (near 1, far 11)
and a single large point: z=3 stays byte-identical to the analytic baseline,
z=8 behind the surface disappears completely, and z=5.5 halves the center alpha
within the one-unit band (`out/w14-soft-probe.log.runs/1789610115439752500.log`).
One retained intermediate failure: the first run hit
`render.gpu_particle_center` because z shared the canvas [-4,4] validation
(`out/w14-soft-probe.log.runs/1789610005228101500.log`).

Runtime regressions `gpu_particles`, `gpu_sampling`, `gpu_particle_graph`,
`event_gpu` and `work_library` pass
(`out/w14-soft-regression.log.runs/1789610131702418400.log`). Windows SM5 and
Android GLES 3.0 shader compilation pass (`out/w14-shader-check/`). Android
device revalidation stays in the final platform phase.

Remaining W1.4 work: layered volume presentation exercised in an authored work
with observable low/mid/high-frequency roles and silent autonomous motion.
