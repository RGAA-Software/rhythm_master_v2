# Other-window mouse releases canceled output editing

During P4 from-empty Studio acceptance, a work created through the node palette,
property inspector and named bindings rendered correctly, but a direct output
drag returned to its original position on release. The saved `translate_x` was
zero. This was found internally, not reported as a successful workflow.

## Cause

`GraphCanvas::Draw` used the global left-button release to persist all native
node positions. Newly inserted or rounded native positions could differ from
the saved layout even when the gesture belonged to another window. Applying that
layout snapshot canceled the output canvas draft before its release transaction.
The previous GPU fixtures loaded prepared layouts, missing the newly authored
layout condition.

## Change and permanent coverage

The graph canvas now records whether the press started inside its hovered canvas.
Only that gesture's release may persist its layout. Releasing outside the canvas
still completes a drag that started inside; application focus loss clears the
ownership flag. Unrelated releases cannot publish graph layout transactions.

- `canvas_interactions::UnrelatedMouseRelease` retains an unpersisted native
  position and verifies that an outside click leaves the snapshot unchanged.
- Existing node dragging, port connections, connected-node dragging and preview
  dragging remain covered by `canvas_interactions`.
- The developing `from_empty_studio_2d` acceptance creates every work node through
  Studio, verifies each saved named connection, drags the output, encapsulates
  the transform through the component UI, saves, publishes and reopens.

## Evidence

- Failed before the fix: `out/p4-from-empty-drag-evidence-tests.log`;
  `out/windows-release/from-empty-studio-2d/33009a0ce71a408b8e6bc2849156a14e/`.
  Actual saved translation: zero. Changing window focus alone did not fix it.
- Passed after the fix: `out/p4-cross-editor-release-tests.log` (source boundaries,
  actual from-empty Studio flow, canvas interactions).
- Studio evidence:
  `out/windows-release/from-empty-studio-2d/0a2cef4505c94e959786cddf707706ad/`.
  Saved horizontal translation approximately 0.0998 for a 0.1-canvas drag;
  component publication matches compilation of the saved project.
- Six existing GPU authoring workflows passed:
  `out/p4-cross-editor-regression-tests.log`.
- Studio delivery, automatic 20-DLL deployment and all four mandatory template
  checks passed: `out/p4-cross-editor-release-delivery.log`.

This evidence establishes the interaction fix. It does not establish musical
response, visual quality, Android parity or the complete P4 exit works.
