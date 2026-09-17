# GPU point layered volume validation — 2026-09-17

W1.4 third increment: layered particle volume exercised in an authored work,
closing W1.4 (atlas `provenance/gpu_point_atlas.json`, soft depth
`provenance/gpu_point_soft_depth.json`).

`aureate_vortex` is the only authored work that already combined
`scene.capture` + `scene.depth` + `texture.dof` with two GPU particle chains,
so the layered-volume increment lands there (`tools/concept_spatial_works.py`).
The shared `render()` helper now returns the scene depth alongside the image
(all four call sites updated; porcelain/corridor pass no focus and receive
`None`). A compiled 2x2 `texture.shader` expression atlas provides soft dot,
thin ring, crossed streak and hot spark quadrants; every shape is symmetric
and reaches zero at its cell border, so per-particle quadrant selection shows
no seams and no host aspect ratio can distort the mapping. The dust layer
(24576 points) emits at center z 10.6 behind the vortex arms and only shows
through the gaps; the spark layer (6144 points) emits at 8.8 in front and
fades against the nearest arm silhouettes (`soft_distance` 0.9/0.7). Both
`gpu.render` nodes consume the same scene depth that feeds `texture.dof`.
Graph grows 176 to 177 nodes; the published package reports runtime ABI 5,
177 instructions, 7 assets.

Retained intermediate failures: the first atlas expression used
`vec4(vec3(1.0), ...)` and failed D3DCompile X3014; rewritten as
`vec4(1.0, 1.0, 1.0, ...)`. The first manual music-gpu run expected 162
reachable instructions; the actual count is 177, so the stale
`luminous_event_music_gpu` expectation (161, predating several graph
revisions) was corrected to the measured 177 in
`src/windows_spike/CMakeLists.txt`.

Actual paths exercised:

- Authoring regeneration of all five concept works
  (`python tools/author-concept-works.py`); non-aureate templates are
  byte-identical, their provenance authoring-source hashes track the changed
  generator files and this host's CRLF checkouts.
- Manual `tools/test-music-gpu.py` on the real package, 1280x720 D3D11
  captures (`out/w14-aureate-music/`): mean pixel differences
  resonance_demo/silence 2.90, low/silence 6.33, high/silence 0.41,
  low/high 6.04, all far above the 0.15 floor — low/mid/high roles remain
  distinguishable and the silence frame keeps the full vortex with both
  particle layers (autonomous motion intact, soft sprite edges, no hard
  blocks or ghosting on inspection at native resolution).
- Final combined ctest sweep 7/7
  (`out/w14-aureate-final.log.runs/1789614436874108600.log`):
  `luminous_event_music_gpu`, `calibration_aureate_vortex_replacement_gpu`,
  `aureate_vortex_export_ui_gpu` (Studio arranged-template export path),
  `export_failure_ui_gpu`, `scene_creation_gpu` plus both fixtures.

Regression found by this verification (pre-existing, unrelated to the atlas
and soft-depth runtime paths): the bgfx depth-of-field private buffer cache
(`BgfxDepthOfField::buffers_`, seven textures per extent, 8,812,800 bytes at
960x540) was only released with the renderer, so any work using
`texture.dof` failed the scene-transition baseline after
`deck.ReleaseGraphics()` (`scene_creation_gpu`,
`src/player_core/tests/scene_compositor_probe.cpp:164`). Generator bisection
proved the leak byte-identical with atlas and depth ports removed; the
ResourceTable alloc/free trace balanced 46/46. Unused extent groups are now
released at frame end with the same epoch semantics as the runtime
`TexturePool` (`src/rhythm_render/src/bgfx_depth_of_field.*`,
`bgfx_backend.cpp` EndFrame); `scene_creation_gpu`, the gpu particle
regressions, `depth_runtime`, `scene_compositor_gpu`,
`scene_replacement_gpu`, `preparation_gpu` and `windows_effects_gpu` all
pass. One noted flake: the first post-fix run failed
"complex incoming work takes over" on a 41.9 ms first-time shader compile
spike; four consecutive runs pass with warm caches. `docs/depth_pipeline.md`
now records the frame-end retirement.

Host migration note: the deploy gate flagged the media DLLs because
`out/vcpkg-media-lgpl` was rebuilt on this host on 2026-09-16 from the
validated manifest (FFmpeg n6.1.1#11, port tree 15b90b33, zlib 1.3.1) while
`provenance/media_lgpl_windows.json` pinned hashes from the previous host.
The documented evidence suites were re-run against the rebuilt binaries —
video (`out/media-lgpl-video-tests.log.runs/1789611853628441900.log`), audio
(`.../1789611854312298000.log`), playback
(`.../1789611855093690500.log`), all green — and
`tools/prepare-media-notices.py` re-measured the profile: identical LGPL
license string and configuration flags (only install paths changed), new
Release/Debug DLL hashes and a regenerated source archive. The Studio deploy
validation then passed and `rhythm_master.exe` deploys again.

Android device revalidation stays in the final platform phase. W1.4 is
closed; remaining particle work moves to W1.5 temporal/recycling review.
