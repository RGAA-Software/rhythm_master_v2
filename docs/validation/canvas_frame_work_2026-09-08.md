# Avoid redundant canvas snapshot and descriptor copies

The node canvas copied its full editor snapshot at entry and again when no link
was accepted. Idle, hovering and panning frames therefore copied nodes,
properties, layouts and embedded components twice. It now creates a transaction
only for an accepted connection/deletion or changed authored position. Link
validation retains its existing command semantics; an unaccepted candidate is
discarded. Multiple changes within one draw share the pending transaction.

Operator descriptions are also resolved once per type per draw, instead of once
per instance. The cache is deliberately local to that draw: edits to component
ports, defaults and types are visible on the next frame without a new persistent
cache invalidation protocol. Native node rendering and third-party node-editor
interaction behavior remain delegated to the existing adapter.

## Validation and measurements

`out/canvas-frame-work-tests.log`: source boundaries, actual ImGui canvas
interactions, component edits and component timing in both locales pass (5/5).
Coverage includes 1,000 nodes, title/body dragging, connected node movement,
port connections, deletion, right-pan/cursor, numeric previews and hidden previews.
The change does not add a new public runtime contract or Android dependency.

Actual Studio benchmark: 290-node Harmonic City, 1920×1080, real decoded music
muted after analysis, eight previews, 600 measured frames after 120 warm-up frames.
It exercises hover, zoom and 39 right-pan frames. Rendering retains the same
60,594,180-byte peak texture use and one recycled target.

| Implementation | Editor + graph p50/p95 ms | Whole host p50/p95 ms |
| --- | --- | --- |
| Before | 12.4131 / 17.9127 | 22.0213 / 31.6357 |
| Lazy transaction | 8.6978 / 13.1653 | 16.6645 / 25.7061 |
| Plus per-draw type descriptions | 7.6777 / 11.5193 | 16.2950 / 23.6310 |

Logs are `out/harmonic-city-editor-before.log`, `-after.log` and `-final.log`.
The music clock follows device consumption, so the same frame count does not
sample exactly the same song instants. Unrelated user build activity existed in
the first two runs and had finished before the final run; no unrelated process
was terminated. These observations establish the remaining cost, not a controlled
percentage speedup attributable exclusively to these two edits. The redundant
copies are independently visible in the removed code. Stable 60 fps is still not
established for this scene/editor workload.

`out/harmonic-city-export-final.log`: the actual Studio button exports another
480-frame H.264 MP4 while the editor continues rendering. Parent p50/p95 is
23.3419 / 32.2747 ms across 170 measured frames. The earlier repeat before this
fix reached 39.3329 / 56.7724 ms under unrelated build activity, recorded in
`out/harmonic-city-export-repeat.log`. Export retains noticeable frame cost;
neither number is a GPU execution time or a guarantee of hitch-free editing.

`out/canvas-frame-work-build.log` rebuilds and automatically deploys Studio with
all 20 DLLs and resources. Player and Android binaries from the preceding scene
increment remain applicable because this change is private to Studio's canvas.
