# Intermediate texture lifetimes and profiling, 2026-09-08

Dynamic RGBA8 outputs can now return their owned targets to an extent-keyed
pool after their final consumer has submitted. Static caches, final output,
observed node previews, temporal history and potentially aliased blur/trail
inputs remain retained. This preserves GPU submission order and clears retired
public handles. Runtime callers retain all outputs by default; Studio and
Player explicitly enable reuse. Studio exposes a reuse toggle and opt-in CPU
node profiling in the inspector. CPU submission time is not GPU execution time.

The schedule includes feedback writes after evaluation. Plan content, not just
revision, invalidates it. Paused identical frames avoid recomputation. Pool
entries not used by the current evaluated frame are released; resource-budget
recovery remains bounded and retryable.

## Reuse decision

Studied https://github.com/google/filament at
`97651a0ff4757162cf20848a1599907f15e423ed`, Apache-2.0:
`filament/src/fg2/FrameGraph.cpp` (first/last active use) and
`filament/src/ResourceAllocator.cpp` (descriptor-keyed allocation cache).
No source was copied, no dependency added, and no upstream files modified.
Filament's DriverApi/FTexture and graph arena do not fit Rhythm's value
ExecutionPlan, owned Texture/framebuffer pair, cross-frame static caches,
feedback aliases and independently observed node outputs. The adaptation uses
the existing Rhythm/bgfx RAII boundary and a project-specific lifetime schedule.

## Evidence

- Windows tests: `out/lifetimes-final-tests.log`, 7/7 including source
  boundaries, runtime/lifetimes, real D3D budget recovery, decoded music and
  complete template catalog GPU rendering.
- 84-node synthetic graph at 1024 square uses at most 28 MiB, retains requested
  previews, releases obsolete pool targets and submits no passes on repeated
  paused frames. It would exceed the 256 MiB budget with all targets retained.
- Four deterministic 1280x720 music/silence/low/high captures are pixel-exact
  against the previous Player: `out/lifetimes-image-comparison.json`.
  Player texture bytes fall from 215,094,788 to 93,443,588.
- Same Windows editor benchmark binary, 164 nodes, 8 previews: texture bytes
  249,828,868 retained versus 157,668,868 reused. Median host frame time is
  16.52 versus 16.47 ms; p95 is 17.19 versus 18.48 ms. Graph/UI CPU time does
  not improve reliably. Audio samples vary with real-time playback, so these
  are not a deterministic CPU comparison. Logs: `out/lifetimes-ab-*.log`.
- Android e2b3b128, API 34, Adreno 650: native runtime/lifetime tests and all
  GLES contracts pass (`out/lifetimes-android-tests.log`). A 26-node graph
  including zero-radius blur, alternating exposures and feedback matches
  every pixel for 32 frames, including pause; bytes 24,580 versus 6,148.
- Resonance Gate, 960x540, 120 warm-up + 300 synchronized offscreen frames:
  before 30.2894/33.6414 ms p50/p95 and 120,998,468 texture bytes; after
  24.8768/30.6705 ms and 52,569,668 bytes. Logs:
  `out/lifetimes-android-before.log`, `out/lifetimes-android-after.log`.
  This is synthetic-feature native rendering, not APK audio acceptance or a
  stable 60 fps claim.
- Windows Studio/Player Release bundles rebuilt and Python-deployed, each with
  20 DLLs and resources (`out/lifetimes-delivery-build.log`).

Reusing dynamic intermediates can cause recomputation when an input happens
to remain unchanged. The inspector toggle permits retaining those caches.
General pass fusion, GPU timestamps and full large-graph scheduling remain
future performance work. APK installation was rejected by the phone with
`INSTALL_FAILED_USER_RESTRICTED`; native test access succeeds. Neither this
performance increment nor an APK alone completes the product roadmap.
