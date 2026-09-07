# Firefly Garden editor CPU diagnosis

The acceptance bundle previously used the Windows Debug cache (`/Od /Ob0`,
debug CRT/STL). Particle simulation, GLM curl-field sampling, graph evaluation
and ImGui construction run on the host thread. Expensive CPU frames postpone
event processing and GPU submission. Low GPU utilization therefore does not
imply that editor interaction is inexpensive. The screenshot's system CPU
percentage is not a measurement of this process alone.

## Controlled comparison

Reference machine: Ryzen 9 5900X, 24 logical processors, GeForce RTX 3060,
Windows D3D11. No builds or other project benchmarks ran during measurement.
Other desktop/background processes were not terminated or controlled.
The same current Firefly Garden template and effect implementation were used
in both configurations. No particle count, simulation rate, render resolution,
trail quality, font or preview quality reduction was applied.

`editor_benchmark` creates an isolated project, opens the actual Studio at
1920x1080, moves the pointer across the canvas and retains all eight inline
previews. It runs 720 frames, discards 120 warm-up frames and measures 600.
Project time advances at 60 Hz; audio input is silent. These are host wall-clock
durations, not GPU timestamp measurements or audio-device latency measurements.

| Measurement | Debug p50 / p95 ms | Release p50 / p95 ms |
| --- | --- | --- |
| Whole editor frame | 33.8524 / 40.5851 | 16.6618 / 16.9676 |
| Studio graph + preview + UI construction | 32.2977 / 38.7431 | 2.9991 / 5.1966 |
| UI translation/submission + present | 1.6031 / 2.3466 | 13.7639 / 15.7228 |

Release enables `/O2 /Ob2 /DNDEBUG`. With CPU work below the refresh budget,
the present-phase timings are consistent with waiting for the existing 60 Hz
vertical synchronization; they do not isolate GPU execution time. The full frame
reaches approximately 60 fps in this test. This does not establish performance
for arbitrary graphs, 4K output, another machine or concurrent compile workloads.

The separate effects-only benchmark (1280x720, synthetic canonical audio,
600 measured frames) changes from 29.5864 / 38.0558 ms to
16.6598 / 16.8350 ms. Both retain 85,437,188 texture bytes without growth.
Inline texture previews reuse graph outputs at a 15 Hz capture budget; they
do not each run another complete particle simulation.

Evidence: `out/editor-before.log`, `out/editor-release.log`,
`out/performance-firefly-before.log`, `out/performance-firefly-release.log`.

## Delivery and verification

`python tools/build-windows.py` now defaults to Release, 20 workers and the
validated vcpkg SDK locations. `out/windows-release` is a separate incremental
cache; `out/windows` remains Debug. Configuration mismatches are refused instead
of silently repurposing a cache. Existing Python deployment targets run after
application builds, including no-op builds. `tools/run-studio.ps1` defaults to
the optimized bundle.

- Studio: `out/windows-release/src/windows_spike/deploy/rhythm_master.exe`.
- Player: `out/windows-release/src/windows_player/deploy/rhythm_player.exe`.
- Each includes 20 Release DLLs, resources, notices and matching FFmpeg source
  materials. The earlier 21-DLL count refers to the separate Debug configuration.
- `out/windows-release-build.log`: selected targets built with no compiler warnings.
- `out/windows-release-acceptance.log`: 8/8 checks passed, including particle
  determinism, render contracts, video fixtures, actual image/video D3D pixels,
  300-frame popup regression and system-only-PATH Studio deployment startup.
- `out/windows-release-player-smoke.log`: deployed Player startup passed.
- `out/windows-release-noop.log`: the default Python command completed an
  incremental no-op build and refreshed both complete Release deployments.
  Requesting Debug in the existing Release cache was also rejected before
  configuration, as intended.

Reproduce with a **new** output directory for each benchmark run:

```powershell
python tools/build-windows.py --target editor_benchmark
out/windows-release/src/windows_spike/editor_benchmark.exe out/windows-release/src/windows_spike out/windows-release/content/templates/firefly_garden out/performance/editor-repeat.rhythmproj
```

Do not start performance measurements while compilation is still active.
The broader five-item product/content/Android program remains in progress.
