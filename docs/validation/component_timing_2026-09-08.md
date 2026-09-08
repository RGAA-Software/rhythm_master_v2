# Component curve and section editing

The component workbench now contains **组件时间编辑 / Component timing** in its
inspector. Authors can edit internal curves and section bars, choose a visible
time range and add an envelope at an explicit insertion time. Existing local
time wiring remains intact; newly added sections reuse a `core.time` inside the
current component or create one there. Authors can wire `time.local` explicitly.
The main timeline retains playback, music transport and waveform ownership.

`TimeTrackEditor` owns graph curve/section draft transactions and is shared by
the root timeline and component workbench. It has no decoder, audio device,
playback clock or project history. `TimelinePanel` owns transport/waveform only;
`ComponentWorkbench` applies finished internal edits through `ComponentEdit`.
This is focused reuse of the existing graph commands, curve editor, section bar
interaction, scoped ID allocation, history and preview compiler. No upstream
code or dependency was imported; the existing Dear ImGui MIT attribution and
[section reuse record](../../provenance/time_sections.json) continue to apply.

During a timing gesture, the shared component preview publishes a value document
while the original project remains unchanged. Inspector and timing transactions
exclude each other. Release commits one component-history step. Apply, undo,
navigation and closing the timing panel finish the current draft consistently;
canceling the workbench discards it. Applying the workbench remains one root
history transaction. Undo creates a fresh revision while restoring old content.

## Verified boundaries

`out/component-timing-final-tests.log` contains eight passing checks:

- Actual ImGui input with complete English and Chinese catalogs moves an
  envelope connected to an explicit half-speed local clock. Live preview changes
  before Apply, with one undo/redo per drag and unchanged local-time connections.
- Adding reserves a component-local ID, reuses its clock and leaves root nodes
  untouched. One undo removes the addition. Apply and one root undo restore the
  full original content/layout; cancel leaves the original project unchanged.
- Existing component editing, root timeline, section interactions and source
  boundary checks pass after extraction of the shared editor.
- Existing actual Studio D3D11 component previews, numeric traces, cancellation
  and the combined eight-viewer budget pass over 210 frames.

`out/component-timing-layout-tests.log` additionally records five passing checks
after placing the timing entry above the potentially long public-interface
form. A real Studio fixture connects an internal envelope to a 3D material,
opens the timing panel and asserts that its section child is actually visible.
The 210-frame run retains the combined eight-viewer limit and reaches a peak
texture allocation of 152,153,092 bytes. The inspected screenshot is
`out/component-timing-review.png`; the editable fixture is
`out/windows-release/component-timing-project/`. Windows deployment is updated.

This edits graph timing inside a component. It does not add an independent
component playback transport, local waveform mapping, cue-event tracks,
multi-track audio, automatic conversion between unrelated time inputs or
stateful simulation preroll. Range/insertion view settings are session-local;
node curves, intervals and connections persist with the component definition.

No Android runtime contract changed in this UI increment. Its time/local-time
operators and portable component commands were already exercised on the USB
phone; a Windows workbench test does not count as Android application acceptance.
