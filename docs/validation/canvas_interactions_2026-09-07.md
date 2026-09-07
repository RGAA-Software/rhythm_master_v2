# Canvas interaction repair — 2026-09-07

User acceptance: on 2026-09-07 the user accepted the updated Windows build.
This records acceptance of the delivered slice, including the interaction and
visual changes below; outstanding whole-product capability/risk gates retain
their own status. The accompanying Apple-last decision is recorded in the
Phase A execution plan and platform roadmap.

Node-editor's internal hit widgets hash numeric IDs without the Node/Pin/Link
type. The previous adapter passed authored node/link IDs directly while pins
used another counter starting at 1. For example, dragging node 3 could activate
node 2's output pin 3 and begin a connection instead of moving the node.

The adapter now allocates all UI object IDs from one identity space. Selection,
deletion and position updates map back to authored IDs, which remain unchanged
in project files. No upstream source changes are required.

Right-button canvas drags use the hand cursor and restore the arrow on release.
Only the circular sockets start links; titles, labels and body space can drag
nodes. Node presentation lives in a separate private widget module, with colored
headers, type-colored sockets/links, left inputs, right outputs and selection
borders. The built-in template uses a compact three-column layout to keep the
initial fitted view legible. Existing saved layouts are preserved.

`canvas_interactions` feeds mouse/key events into the real GraphCanvas and
node-editor through ImGui's input queue. It opens no OS window and does not move
the user's pointer or edit a saved project. Cases cover colliding authored IDs,
node dragging/selection, port connection, moving a connected node, deletion
mapping, right-button view translation and cursor release. Panning must preserve
the graph and authored node positions.

Restoring the old independent ID allocation temporarily makes the node-3 drag
case fail; restoring the fix makes it pass. The negative-control log is
`out/canvas-collision-regression.log`. Full Windows checks include this test and
the deployed GPU smoke test with a system-only PATH. Visual evidence is
`out/studio-nodes-full.png`; build/test evidence is `out/windows-build.log`.
