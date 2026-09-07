# Named graph bindings

Inspector now exposes graph-scope names for a selected output and lets compatible
inputs reference those names. Reusing a name changes its source transactionally;
type/cycle failures preserve the original snapshot. Choosing a wire replaces the
binding and vice versa. Bound constants are disabled, and canvas input labels
show the referenced name. Removing an exported name or its source clears its
consumers; history can undo the operation.

Compilation resolves names to ordinary edges before type checking, demand
traversal and cycle validation. Compiled Player packages carry resolved slots,
so they require no runtime string lookup. Authoring schema 3 stores the names
and consumers with unknown-field retention; schema 1/2 still read, and an old
schema carrying binding fields is rejected. Compiled ABI stays 2.

Limits: 256 names, 4096 bindings, 128 bytes/name, and at most 40000 combined
connections. Protobuf preflight checks repeated records and strings before
allocation. Template instantiation remaps both signal sources and consumer IDs.

Evidence: `out/bindings-full-windows.log` passes 46 suites;
`out/bindings-device-android.log` passes 37 native suites on e2b3b128 under
`/data/local/tmp/rhythm-master-phase-a-20260907030257223`. Domain/editor/persistence
tests cover resolution, demand order, type/conflict rejection, indirect cycles,
retargeting, undo, template remapping, unknown fields, wire limits and compiled
round-trip. Existing canvas interaction tests and application smoke checks pass.

The Shared Pulse template controls scale and gradient amount through one name.
Its published Windows package plays 30 Windows and 60 Android GPU frames;
logs: `out/bindings-player-windows.log`, `out/bindings-player-android.log`.
There are 41 presets and 12 templates. This is graph-scope binding support;
nested component scopes and reusable semantic components remain outstanding.
