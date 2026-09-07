# Embedded component validation

Component definitions now contain typed public inputs, grouped public parameters
with optional narrowed ranges, named bindings and nested component instances.
Definitions are embedded snapshots in authoring schema 4. Schema 1–3 reading
and compiled ABI 2 remain supported; publishing expands components to ordinary
instructions. No component lookup or authoring library reaches Player execution.

Expansion preserves each instance's output node ID for inline viewers, isolates
parameter values and replaces internal default connections when a public input
is connected. Limits cover 256 definitions, depth 16, 10,000 stored/expanded nodes
and 40,000 expanded edges. Recursive definitions and invalid public interfaces
are rejected. Unknown Protobuf metadata round-trips.

Studio can wrap a selection with one observable output, add instances from the
project library, edit grouped public controls and expand the project for debugging.
These operations are undoable and preserve retired node IDs. The Pulse Card
Component template exposes local time, rotation, pulse and palette controls.
The library contains 41 presets and 13 templates; this does not satisfy the full
40 semantic node / 120 preset / 24 template content milestone.

Evidence: `out/components-full-windows.log` (49 suites),
`out/components-device-android.log` (40 native suites on e2b3b128),
`out/components-player-windows.log` (30 real GPU frames) and
`out/components-player-android.log` (60 real GLES frames of the Windows package).
Device artifacts: `/data/local/tmp/rhythm-master-phase-a-20260907034444414`.
Python deployment updated both Windows deploy directories and rebuilt the APK.
APK installation and Java-host lifecycle/audio acceptance remain pending.

The following authoring increment adds a draft window with nested breadcrumbs,
internal canvas/property editing, grouped public input/parameter exposure, hiding,
reordering and range editing. Applying checks the original project revision;
invalid drafts and conflicts preserve the current project. Draft undo is shared
across nested scopes. Hiding parameters removes instance overrides; hiding a
connected port is rejected until its consumers are disconnected.

Detaching an instance clones its entire nested definition closure and layout,
so other instances keep their existing definitions. Internal node positions are
editor schema 2 metadata and round-trip through project save/load; editor schema 1
remains supported. The layout codec rejects invalid IDs, duplicate scopes and
oversized metadata before replacing the committed revision. Layouts are not part
of the compiled Player package.

Evidence: `out/component-layout-full-windows.log` (51 suites),
`out/component-authoring-device-android.log` (41 native suites), and
`out/component-layout-device.log` (updated layout/edit contracts on Android).
Latest device directory: `/data/local/tmp/rhythm-master-phase-a-20260907040043802`.
UI tests exercise simultaneous canvases and draft apply/close through real ImGui
input queues. Windows deploy directories are current. Native APK capabilities are
unchanged by this Studio-only layout increment.

Individual expansion, internal draft viewers, reusable user component presets
and official update/migration workflows remain subsequent work. Current components
are embedded snapshots; no automatic upstream update is performed. Full timeline
authoring and the rest of step 3 remain incomplete.
