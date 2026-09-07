# Reused visual effects and the first template candidates

This increment adds reusable effects and two Basic template candidates. It does
not complete steps 1–7 or establish visual acceptance toward 50 Basic and 50
Advanced templates. Current inventory is 24 runnable examples, eight catalog
semantic nodes and 105 preset records (89 operator + 16 semantic presets).
Embedded components inside the two new templates are not counted again as
catalog semantic nodes or as extra templates.

## Source reuse and boundaries

TiXL revision `fbc994d923e8a0142d2ff1b772e4d12248c5b0ba` was verified from the local
reference checkout. Three focused upstream shaders were copied unchanged into
`third_party/sources/tixl-effects`, alongside the original MIT license:

- `Bloom-SeparableBlurPS.hlsl`: normalized five-bilinear-sample Gaussian kernel.
- `Bloom-DownsamplePS.hlsl`: four-sample downsampling.
- `PerlinNoise2d.hlsl`: gradient-noise, hash, interpolation and fractal reference.

`provenance/tixl_effects.json` records original paths, exact hashes, revision,
destinations and adaptations. Original copyright, license and attribution are
also included in the Studio/Player notice bundles. Material Maker's Gaussian
nodes were inspected as an alternative; their generator-specific function inputs
and 101-sample loop were unnecessary for this bounded runtime filter.

The adapted shaders use bgfx bindings and project value commands, without TiXL's
C# host or public Direct3D types. Imported algorithm naming is preserved inside
shader adaptations. No shared vcpkg packages were changed. Shaderc remains the
previously validated host-tool exception. Preview encoding uses the existing
vcpkg `x64-windows-static-release/tools/ffmpeg/ffmpeg.exe` as an offline tool only;
the pending Studio/Player media linkage and outbound-license decision are unchanged.

## Implemented behavior

- `texture.blur`: optional scalar radius input, parameter fallback, zero-radius
  bypass, bounded downsample pyramid plus horizontal/vertical Gaussian passes.
  Radius is measured relative to a 720-pixel short edge, preserving appearance in
  inline previews. At most six downsample levels are allocated. A stable graph
  reuses textures; extent/level changes replace intermediates; RAII releases them.
- `texture.noise`: aspect-correct spatial gradient noise with a fixed four-octave
  fractal, bounded scale/contrast/seed and optional phase input. Phase is a graph
  value, not a shader-owned clock. Identical input yields repeatable output on the
  tested backend. Cross-device bit identity is not claimed.
- Premultiplied RGBA filtering and palette interpolation prevent colors stored
  under zero alpha from bleeding into output. Mutually exclusive shader commands
  and invalid numeric values are checked at the render boundary.
- Both operators appear in the categorized native-node palette, have Chinese and
  English labels, Default and two additional presets, inline previews and normal
  graph/package behavior.

The glow component adds narrow and broad blurred layers to its source. It is a
composable RGBA8 glow, not HDR bright-pass bloom. Highlight clipping, clamp-edge
sampling and the current rendering profile remain explicit limitations.

## Candidates and review artifacts

`content/templates/layered_neon` contains seven root nodes and three editable
components: neon core, stardust and layered glow. Audio spectrum and RMS respond
to canonical audio input; rings, orbit lines and particles still move in silence.
The current palette and orbit geometry remain subject to visual review.

`content/templates/aurora_clouds` contains a two-node root graph and an editable
ten-node component: two independently evolving noise layers, color adjustment
and glow. This is an atmospheric background candidate, not a completed advanced
visual showcase. Both templates include source README files with controls/limits.

Open `out/effects-review/index.html` for the actual rendered videos, reference
frames and published packages. Each video is six seconds at 960×540 / 30 timeline
frames per second. `preview.json` records the package hash and synthetic input.
Neon uses explicitly synthetic canonical audio features; this is not live-capture
or music-file-decoding evidence. Aurora ignores audio. Capture rate does not
establish sustained device performance or a frame-rate guarantee.

Fresh editable review projects are available at:

- `out/effects-review/layered_neon.rhythmproj`
- `out/effects-review/aurora_clouds.rhythmproj`

They passed actual Studio smoke runs: 7/7 visible nodes and seven inline previews,
and 2/2 visible nodes and two inline previews respectively, each with 30 GPU frames.
The same candidates are available through Studio's normal template menu.

## Defects found through actual rendering

The initial preview lost static layers while dynamic spectrum remained. Setting
the presentation size before rendering avoided it, exposing the relationship to
the late surface resize. The private bgfx adapter now advances a presentation
generation on resize; runtime caches redraw when it changes. GPU regression
renders a static graph before a surface-size change and verifies that it recovers.
The preview tool primes three startup frames before recording; the independent
resize regression does not hide the problem with a warm-up.

The new template initially used layout version 1 with component layouts. The
template-contract test rejected it. It was corrected to version 2, rebuilt and
the template test passed. The first resize-test graph also omitted its required
output node; that fixture was corrected before the final passing GPU run.

## Verification

- Windows render module and runtime increments built/tested before application
  adoption. Final Studio/Player builds used 20 workers and retained caches.
- `out/effects-integration-tests.log`: ten passing suites — source boundaries,
  render contracts, blur resources/runtime caching, texture command binding,
  template contracts, operator presets, effects GPU, deployment smoke, Studio
  smoke and Player smoke.
- GPU effects verification includes four blur cases (constant color/alpha,
  neutral impulse on transparent red, broad pyramid blur and zero bypass), five
  noise cases (detail, phase, seed, repeatability and transparent palette), plus
  the cached-surface-resize regression. Captures are retained under
  `out/windows/effects-captures`.
- Actual Player preview runs record 21 last-frame passes and 48,422,468 texture
  bytes for neon; 12 passes and 15,228,484 bytes for clouds at 960×540. These are
  observed allocations, not device performance qualification.
- `out/effects-android-build.log`: shared effects, GLES shaders, native contracts
  and Player APK cross-built successfully. ADB currently reports no device, so
  new Android pixel, touch/lifecycle and sustained-performance tests remain open.
  Earlier device-side APK-install refusal still requires resolution before install.
- Both Windows sibling `deploy` directories contain the executables, 16 runtime
  DLLs, content/locales and notices. No new project compiler warnings were emitted;
  an existing external Perl locale warning appears during dependency configure.

The next visual capabilities remain displacement, kaleidoscope and controllable
trails, followed by the rest of the representative Basic/Advanced batch. Content
browser thumbnails/category metadata and the complete 100-template catalog are
still outstanding. Communication remains paused and Apple remains deferred.
