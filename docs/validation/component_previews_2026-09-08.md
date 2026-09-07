# Live component viewers in the shared Studio runtime

The component workbench now displays internal texture, point, geometry, material
and scene viewers through the existing runtime viewer adapters. Root and internal
viewers share the existing eight-image budget, 256×144 images and 15 Hz capture
cadence. Focused component demand has priority; remaining slots serve the root
canvas. No second full graph runtime or GPU-to-CPU viewer readback is introduced.

## Ownership and reuse

- `ExpandComponentScope` extends the existing first-party component expansion
  walk. A concrete instance path maps local body IDs to the same expanded IDs
  used by normal compilation, retaining instance parameters and external inputs.
  Runtime/public render types and the published program format do not change.
- The existing bounded `CompilerWorker` resolves scoped viewer-only demand on
  its worker and publishes the matching map with its generation. A private
  `PreviewRouting` UI component owns budget allocation and plan-commit routing.
  Old mappings and cached images are invalidated when the scope/plan changes.
- The workbench projects its draft through `ComponentEdit::Finish` without
  committing history. Property/interface edits update the live draft; undo,
  apply and cancel keep their existing transactional behavior. An idle draft
  does not continuously request compilation.
- When opening from the project component library, the selected matching root
  instance is preferred, otherwise the first matching root instance is shown.
  Its numeric path is displayed. Nested navigation extends this concrete path.
  A definition without a root instance can still be edited; add an instance or
  navigate through its outer component to obtain contextual previews.
- Definitions still edit all their instances on Apply. This is not a change to
  per-instance component ownership, nor an instance-picker UX for every nested
  use of a shared definition. Graphs with incomplete/invalid draft wiring retain
  compiler diagnostics rather than displaying invented outputs.

This reuses project-owned expansion, compiler, draft, node-canvas and GPU viewer
implementations. No third-party source or new dependency is imported.

## Evidence

- Graph tests cover two instances, nested parameter propagation, externally
  overridden inputs, stable output identities and invalid/deleted paths.
- Compiler tests verify a viewer-only branch inside a component is included in
  the shared plan and its returned mapping belongs to the completed generation.
- Real ImGui tests verify inline image demand, idle compilation stability,
  interface draft changes, undo and closing the draft. Routing tests cover the
  shared budget, deferred resource commit and stale-mapping invalidation.
- `component_preview_gpu` opens the actual Release Studio on Resonance Live,
  activates the component workbench through ImGui, displays seven internal
  viewers, captures the UI and cancels the draft. It runs 210 actual D3D11 frames
  with the total viewer count bounded to eight and no render budget failure.
  See `out/component-preview-regression-tests.log` and the inspected capture
  `out/windows-release/component-preview-project/component-preview.png`.
  This short test is not a long-run, audio-latency or sustained-FPS claim.

Scalar plots, specialized camera/configuration viewers, more complete instance
navigation and automatic migration of official definitions remain separate
authoring work. The complete five-stream and 50 + 50 program remains unfinished.
