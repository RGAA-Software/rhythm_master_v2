# Resonance Live: complete editable performance increment

This is an additional Advanced candidate, not completion of the Basic 50 /
Advanced 50 target or TiXL/TouchDesigner-class authoring roadmap. There are now
30 template examples. Final user visual acceptance and Android APK acceptance
are still pending.

## Composition and reuse

`tools/author-resonance-live.py` reuses the first-party Resonance Gate builder
without changing the existing 164-node example. The new performance has nine
root nodes, two embedded editable components, 190 component-body nodes and 197
reachable executable instructions. A three-second smooth opening curve follows
the common music time. The field contains the 24-band blades, radial spectrum,
orbital outlines, procedural field and particle trails. The core adds four
interlocking PBR torus instances, bass-driven scale and loudness-driven emission.
Both components expose bounded controls and retain internal canvas layouts.

No upstream code or assets are copied for this composition. The existing
Godot-derived torus primitive and previously attributed effect adapters are
reused unchanged; their existing provenance/notices apply. No new dependency,
media backend, platform assumption or outbound license is selected.

## Reproduction and editing

1. Build the Release Studio/Player with `python tools/build-windows.py`.
   Both sibling `deploy` directories include the executable, DLLs and content.
2. In Studio's template selector choose **星门演出 / Resonance Live**.
3. Start the demo music or a local audio file. The timeline follows consumed
   music time; pause, seek and restart also control the opening curve. A seek
   starts fresh particle/trail history; it does not reconstruct earlier frames.
4. The field exposes `pulse` and `exposure`; the core exposes `pulse` and
   `emission`. Open the component workbench to edit internal nodes. Save the
   selected component in **我的组件库** to reuse it in another project.
5. Save/reopen the project, publish its runtime package, then open it in Player
   with the same music. Music selection is currently host-local; it is not
   embedded as a project arrangement track or packaged soundtrack.

Component workbench internal viewers have subsequently been connected to the
shared runtime; see `component_previews_2026-09-08.md` for their instance scope,
draft behavior, budget and validation. Encoded audio/video export remains open.

## Verification

- `out/resonance-live-tests.log`: catalog load/runtime and actual D3D11 rendering
  with FFmpeg-decoded demo, silence, low tone and high tone all pass. Every
  expected instruction is reachable. Texture allocation is stable after warmup.
- `out/windows-release/resonance-live-music-captures`: actual decoded PCM PNG/TGA
  captures. Mean absolute RGB-channel differences on the 0–255 scale are
  demo/silence 10.8764, low/silence 16.1616, high/silence 8.1799 and low/high 16.6493.
  These are visual-response evidence, not a hardware audio-latency measurement.
- `out/resonance-live-review/829182d22ca345c19c6c75d3c2a2bb2f`: inspected 960×540
  capture and six-second MP4 with synthetic features, explicitly distinguished
  from decoded PCM. The catalog thumbnail also comes from actual D3D11 rendering.
- `performance_workflow` tests the shared authoring APIs: exposed control edits,
  undo/redo, save/reopen of curves and internal layouts, identical published plan,
  component capture/import into an independent document, music pause/seek and
  graphics recreation. The transport portion uses a Null renderer and typed
  music samples; it does not replace actual GPU/audio tests or GUI acceptance.
  It passes on Windows (`out/component-determinism-tests.log`) and the USB phone
  (`out/performance-workflow-android-tests.log`).
- Native GLES short benchmark on e2b3b128 / Adreno 650: 120 warmup + 300 measured
  frames, synchronized with GPU completion. At 960×540, p50/p95 are
  **31.1405 / 34.3779 ms**, stable texture bytes **59,568,068**. At 640×360,
  **16.2413 / 18.7077 ms**, **26,512,388** bytes. See
  `out/resonance-live-android-measure.log` and
  `out/resonance-live-android-economy.log`. Neither establishes stable 60 fps,
  presentation performance, actual Android audio playback or thermal endurance.

## Defect found by the complete workflow

Authoring Protobuf serialization used unspecified map order, while the runtime
program codec already requested deterministic order. Multi-property component
captures could consequently acquire different content hashes and duplicate
library entries. Graph encoding and stored node extension records now reuse
the runtime codec's deterministic ordering. Unknown extension fields remain
preserved, and previously stored projects remain readable. Stability is scoped
to the validated schema/Protobuf version, not a universal canonical wire format.

The new workflow repeatedly captures and recaptures the actual multi-property
3D component and confirms that saving its loaded copy selects the same library
entry. Existing unknown-field component/persistence and library tests pass.
