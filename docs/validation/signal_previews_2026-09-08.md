# Live scalar and signal node previews

Studio now displays the evaluated numeric value and a bounded history curve in
scalar and signal nodes, including time, frequency bands and expressions. The
component workbench uses the committed instance-ID mapping and the same runtime;
it does not run a second graph or synthesize values in the UI.

## Behavior and limits

- The existing **eight simultaneous viewer slots** are shared by images and
  numeric plots across root and component canvases. Selected nodes have priority
  within a canvas; the focused component has priority over root demand. This
  iteration does not increase the GPU budget or promise all nodes at once.
- Only visible preview rectangles at least 64 screen pixels wide request work.
  Viewer-only branches retain the existing 15 Hz evaluation schedule.
- Numeric histories hold at most 120 samples per node, use playback time and
  append at most 15 times per playback second. Paused playback updates the latest
  value without scrolling. Seek/reset, committed plan changes and hidden demand
  clear obsolete history. Missing, nonfinite or budget-failed outputs never keep
  stale numeric values. The plot is a sample history, not a calibrated oscilloscope
  time axis; no interpolation or invented samples fill gaps.
- Values use six significant digits. Plot coordinates clamp extreme finite
  expression results to ±1e30 while the numeric label retains the double value.
  Each plot autoscales its retained samples; it is not a shared absolute scale.
- Curves remain node body drag areas. Socket hit rectangles, right-button panning,
  hand cursor and preview toggle keep their existing interaction contracts.

## Reuse

Inspected the already pinned Dear ImGui `PlotLines`, `PlotEx`, `ButtonBehavior`
and `ItemHoverable` implementation at revision
`4806a1924ff6181180bf5e4b8b79ab4394118875` (MIT). Reuse the existing PlotLines API,
ring offset and scaling; no plotting library or copied upstream implementation.
The disabled widget still sets its hover ID upstream, so the project adapter also
uses `SetNextItemAllowOverlap` to preserve node dragging. Existing MIT notices are
retained. See [provenance](../../provenance/signal_previews.json).

The project-owned cache handles graph IDs, immutable frame results, playback
generation and bounded lifetime, which the general UI plotting API does not own.

## Validation

- Windows `signal_previews`: real compiled viewer-only time/oscillator branches,
  200 captures with ring rollover, pause, forward/backward reset, hidden demand,
  missing/nonfinite values, extreme values and capacity rejection passed.
- `canvas_interactions`: real ImGui scalar/signal/image previews, dragging through
  the curve and image, socket linking, panning/cursor, visibility and thousand-node
  canvas regression passed. `component_interactions` checks deferred instance
  mapping and stale-map invalidation for numeric values as well as textures.
  Evidence: `out/signal-preview-ui-tests.log` (4/4 including source boundaries).
- Actual Studio D3D11 on Resonance Live: 210 frames, up to eight internal previews
  and five numeric histories; total simultaneous images + histories never exceeds
  eight. Peak texture allocation 152,153,092 bytes, no budget failure; draft cancel
  closes internal previews and root authored-node count remains nine. Screenshot:
  `out/windows-release/component-preview-project/component-preview.png`.
  Evidence: `out/signal-preview-gpu-tests.log` and CTest LastTest log.
- USB phone `e2b3b128` ran the Android arm64 `signal_preview_tests` executable from
  `/data/local/tmp/rhythm-signal-previews/`; the same runtime/history assertions
  passed. This is shared-core verification, not Android Studio UI or APK lifecycle
  acceptance. Android Studio is not a planned authoring host.
- Incremental Windows `--target all` rebuilt 55 tasks and deployed both apps with
  their 20 DLLs and resources. Nine affected integration tests ran: eight passed;
  the initial-scene smoke check still expected only five images and was updated
  to require five images **and three numeric plots**. That smoke and source
  boundaries then passed (`out/signal-preview-smoke-tests.log`). The actual
  soundtrack Studio save/publish/reopen regression passed in the integration run.
  Formatting checks passed. The Android packaging target verified the existing
  Player APK remains unchanged because this feature adds Studio inspection only.

Apple remains the final platform stage. The phone's APK installation restriction,
large-media container/arrangement work and 50 basic + 50 advanced quality content
remain open and are not closed by numeric previews.
