# GPU execution validation — 2026-09-08

R0 short execution gate passed on Windows D3D11 and USB Android device
`e2b3b128` (22021211RC, API 34, Adreno 650). This verifies these paths on
these backends/devices, not all Android GPUs or completion of R1–R6.

## Executed checks

- One indexed quad, two instance records, one submission: separate red/green pixels.
- Compute writes a dedicated GPU read/write instance buffer, drawing directly from
  that buffer. A second dispatch swaps the colors back without CPU instance uploads.
- Offscreen RGBA8 pixels copied to staging after the draw; wait for the declared
  read completion frame. The host retains destination storage through device
  destruction even if a timeout throws. All native handles have deterministic RAII.
- D3D11: instancing and all three pixel scenarios passed.
- Original GLES 3.0 build: instancing passed; compute is disabled by the compiled
  bgfx feature level. This is not a statement that the phone lacks compute hardware.
- Explicit GLES 3.1 build: instancing and compute write/draw/rewrite passed.
  `tools/build-android.py --gles-version 31` selects this validated candidate;
  omitting the option retains the configured incremental cache. Fresh CMake default
  remains 30. The executable reports actual bgfx backend and capability flags.

## Regression found and fixed

Both GLES 3.0 and 3.1 intermittently produced empty scene pixels. Capturing the
scene target in the submitted frame and waiting for readback reproduced the
failure; it was not specific to compute. The packed D24S8 attachment previously
cleared only depth. Clearing both depth and stencil eliminated the observed failure:
three consecutive full short GLES 3.0 suites (each recreates the device), then the
GLES 3.1 full suite passed. This is an observed driver compatibility fix, not a
claim to have diagnosed the driver's internal implementation. Empty scenes still
submit a touch to clear; nonempty scenes already contain submissions.

Android scene and published-graph fixtures now capture the exact output frame,
including the resized node preview, instead of guessing readiness from four draws.
Expected colors, depth order, clipping, culling, normals, PBR and axis bounds remain
unchanged. Windows scene capture checks also pass.

## Commands and evidence

- `python tools/build-windows.py --target windows_gpu_execution_probe --target windows_scene_gpu_tests`
- `ctest --test-dir out/windows-release -R "^(windows_gpu_execution_probe|windows_scene_gpu)$" --output-on-failure`
- `python tools/build-android.py --target android_gpu_contract_tests --gles-version 31`
- Push only the native test executable to `/data/local/tmp/rhythm-gpu-execution-probe`,
  set executable permission, run once with `--execution-probe` and once without arguments.
- Local logs: `out/gpu-execution-windows.log`, `out/gpu-execution-android31-probe.log`,
  `out/gpu-execution-android31-regression.log`,
  `out/gpu-execution-android30-depth-stencil-0.log` and `-1.log`.

No APK uninstall, app-data clearing, dependency upgrade, backend replacement or
long-duration testing was performed. R1 shared feature delivery will rebuild and
overlay-install the APK. No new project-code compiler warnings were reported.

## Source reuse

Existing bgfx adapter reused directly. Source URL: https://github.com/bkaradzic/bgfx.
Snapshot provenance is `third_party/README.md` and `third_party/source_inventory.json`:
imported from old project revision `118dbc811718836ffb4ca62eec7384605df5be32`,
upstream revision unknown; do not substitute this repository's current commit.
BSD-2-Clause notice remains at `third_party/notices/sources/bgfx/LICENSE`.
Inspected `include/bgfx/{bgfx.h,defines.h}`, `src/{config.h,bgfx_p.h,renderer_gl.cpp,
renderer_d3d11.cpp,bgfx_shader.sh,bgfx_compute.sh,cs_blit_buffer_to_texture.sc}`.
No upstream files imported or modified in this increment. The project probe adapts
existing buffer/dispatch/readback contracts; no second graphics abstraction added.

This snapshot assigns `i_data0` to TEXCOORD31 (configured first instance texcoord),
not older examples' TEXCOORD7. GLES compute shader profile is 310_es, ordinary
render shaders remain 300_es. Compute-write buffers are allocated separately from
CPU-initialized instance buffers because bgfx forbids CPU updates to GPU-write buffers.
