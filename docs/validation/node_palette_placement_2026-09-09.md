# Vacant positions for newly authored nodes

P4 from-empty Studio acceptance exposed overlapping nodes after repeated palette
insertion. Each new node previously used the canvas center as its top-left
position, regardless of occupied space. Prepared template layouts did not exercise
this path. The before image is `out/p4-contour-palette.png`.

The existing node-editor fork supplies `GetNodePosition` and `GetNodeSize`; its
public API does not supply a vacant-slot placement operation. The project-owned
adapter reuses these native bounds and adds a small placement policy. Only an
insertion request computes vacant candidates near the visible center, merging
blocked horizontal intervals on five nearby rows. It chooses the closest free
candidate and never rearranges existing nodes. Per-frame drawing only caches the
bounds it already reads. No new dependency or upstream modification was needed.

The reserved size uses at least 260 × 300 canvas units and the largest existing
node, with 40-unit spacing. This is a default insertion policy, not a full graph
layout engine or a guarantee against future enlargement of node interfaces.

`canvas_interactions::VacantInsertion` repeatedly adds 21 nodes with previews
enabled, checks separation and verifies every existing authored position remains
unchanged. The initial test used preview-sized clearance with previews disabled;
that fixture mismatch failed in `out/p4-vacant-insertion-tests.log`. Corrected
preview-enabled coverage, ordinary interactions, source boundaries and both
200/500/1000-node navigation suites pass in `out/p4-vacant-preview-tests.log`.

The actual from-empty Studio workflow also passes in
`out/p4-vacant-insertion-tests.log`; its after image is
`out/p4-vacant-authoring.png`. Existing tests in that run are recorded separately
above; the whole initial run was not successful.
