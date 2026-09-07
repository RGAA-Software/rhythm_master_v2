# Large graph and real music response: Resonance Gate

The previous Firefly Garden graph did not consume audio features. A moving
audio histogram therefore could not make that composition react. The new
Advanced/audio template **共振星门 / Resonance Gate** explicitly connects
26 audio-band readers (24 radial controls plus bass/treble) and loudness to
scale, angle, opacity, spectrum, particle emission and glow.

## Composition and reuse

- 164 authored nodes, 269 edges; all 164 instructions are reachable in the
  published package. The graph remains expanded and editable, with three banks
  of frequency controls and separate background/orbit/particle/postprocessing
  sections in its saved layout.
- Existing noise, polar/kaleidoscope mapping, contour, affine, spectrum,
  particle, trail and blur implementations are reused. Their existing TiXL,
  Material Maker and GLM provenance remains applicable. No third-party sources
  or additional rendering/audio dependency were imported for this composition.
- `texture.stack` reuses the project's existing ordered quad submission and
  premultiplied Over/Add blending. Up to eight input layers use one target/pass.
  Optional holes preserve layer order. It is available in the composition palette
  and has a default preset and bilingual port labels.
- The initial 183-node version with 24 sequential compositors exceeded the
  renderer's cumulative 256 MiB texture budget inside Studio. Four stacks replace
  those 24 intermediate composites, retaining all frequency controls. The final
  exposure pass brings the delivered count to 164. The budget was not raised.
- `tools/author-resonance-gate.py` reproduces the graph/layout/metadata.
  The real rendered thumbnail and its package/pixel hashes accompany the template.
  Visual acceptance remains pending; this does not complete the 50 + 50 program.

## Music entry and verification

Apply the template, then click **播放演示音乐（原创合成节奏）** in the right-hand
audio panel. The 16-second PCM loop is generated from original oscillators and
percussion envelopes by `tools/create-music-fixture.py`, with no sampled or
third-party music. It is generated into `content/audio` and included in each
Windows deploy. Users can alternatively load their own local audio file or
enable the existing system-audio capture. These are mutually exclusive sources.
The template browser now receives the same current input snapshot as Studio,
so selected audio-reactive previews can respond to active music too.

Studio also accepts `--audio local-file` alongside `--project directory`.
The fixture is a separate demo soundtrack, not an embedded soundtrack in the
published graph package or a completed shared media timeline.

`music_gpu_tests` decodes actual WAV PCM through the selected vcpkg FFmpeg
adapter, analyzes it through the existing canonical FFT analyzer and renders
the real package in D3D11. At the identical scene time, RGB mean absolute
differences on the 0–255 scale are:

| Comparison | Mean RGB difference |
| --- | ---: |
| Original demo / decoded silence | 10.5497 |
| 80 Hz / decoded silence | 14.9350 |
| 3500 Hz / decoded silence | 7.8698 |
| 80 Hz / 3500 Hz | 15.4015 |

The test also checks nonzero decoded tone RMS, the complete compiled node count,
at least 24 band readers and stable texture allocation after warm-up. These
captures use real decoded PCM, not manually fabricated spectrum values. They
are offline rendering tests, not measurements of microphone/system-capture or
audio/video presentation latency.

Evidence: `out/windows-release/music-gpu-captures/`,
`out/resonance-final-tests.log`, `out/resonance-delivery-tests.log`.

## Editor performance and 1000-node interaction

Reference: Windows Release `/O2`, Ryzen 9 5900X, RTX 3060, D3D11.
1920x1080 editor window, 1280x720 scene output, 720 frames with 120 warm-up and
600 measured frames. The editor plays the actual demo file through its normal
background decoder/device queue; device volume is zero during automation.
No compilation or other project benchmark ran concurrently.

| Host wall-clock phase | p50 ms | p95 ms |
| --- | ---: | ---: |
| Complete editor frame | 16.5081 | 17.2128 |
| Graph / preview / UI construction | 4.3828 | 7.1125 |
| UI translation, submission and present | 12.0112 | 13.8955 |

Approximately 60 fps in this reference test. Peak visible nodes: 164 in the
overview; peak active inline previews: 8 after zooming. Tiny overview previews
remain culled below the existing 64-pixel threshold. The measured interval
includes wheel zoom and right-button panning (39 observed hand-cursor frames).
Peak audio RMS: 0.236198. Peak tracked textures: 249,828,868 bytes (~238.3 MiB),
including the editor font and previews. Present time includes synchronization;
these are not GPU timestamp measurements or an unlimited-graph guarantee.

`ThousandNodeCanvas` separately drives a 1000-node linked canvas through zoom,
node-header dragging and right-button panning. It asserts that drag changes only
the intended layout and pan preserves graph/positions. This is a canvas input
regression, not 1000 simultaneously rendered effects or a 1000-node FPS result.

Evidence: `out/resonance-editor-final.log`, `out/resonance-final-tests.log`.
The delivered project is `out/review/resonance_gate.rhythmproj`; the package is
`out/windows-release/content/packages/resonance_gate.rhythmpack`.

The cumulative 256 MiB texture and per-frame pass limits remain in force.
Automatic transient-target reuse and larger/high-resolution graph admission
remain future work; increasing output resolution can exceed these limits.
Android native GLES measurements are recorded below; APK music/display and thermal endurance remain unvalidated.

## Checks and delivery

8/8 selected contracts and deployment checks passed, including real PCM/GPU,
1000-node interactions, layer commands, preset coverage and Studio/Player
startup. After browser input forwarding, 5/5 affected checks passed including
the 300-frame catalog regression. Newly changed code compiled without warnings.

Studio: `out/windows-release/src/windows_spike/deploy/rhythm_master.exe`.
Both Windows applications retain complete automatic Python deployment with
20 Release DLLs, resources and the matching FFmpeg notices/source materials.


## Subsequent Android native measurements

USB device e2b3b128 (22021211RC/munch, API 34, Adreno 650), Release GLES.
Each run warms 120 frames and measures 300 with `glFinish`, with synthesized
band/loudness input snapshots. The probe now explicitly rejects missing output
or a budget diagnostic instead of timing a rejected graph as a fast frame.
These are offscreen native execution measurements, not real decoded phone
music, APK display refresh or sustained thermal acceptance.

| Profile | Extent | p50 ms | p95 ms | Stable texture bytes |
| --- | --- | ---: | ---: | ---: |
| Original | 1280x720 | 42.6754 | 45.9465 | 215094788 |
| Balanced | 960x540 | 30.1226 | 34.0716 | 120998468 |
| Economy | 640x360 | 15.3002 | 17.4296 | 53814788 |

Even Economy exceeds a 16.67 ms frame budget at p95; this composition is not
accepted as a sustained 60 fps Android scene. Quality selection remains explicit;
the measurements did not change the scene's effects or the default player profile.
The next performance work should reduce intermediate targets and repeated full
image passes before increasing the authored scene or output resolution further.

Artifacts, including binary/package hashes:
- `out/android-template-measurements/e4af0d68d5c24815883aa04fed2559b6/results.json`
- `out/android-template-measurements/a0c5629fcd764950b1551eb32d88d058/results.json`
- `out/android-template-measurements/db3c8e19782d4e47bf240d017b8f6848/results.json`

See [budget recovery](render_budget_recovery_2026-09-07.md) for editor recovery
when increasing this graph to an extent which exceeds the current resource limits.
