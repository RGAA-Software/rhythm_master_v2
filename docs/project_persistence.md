# Project persistence and Protobuf decision

> Status: revision transactions, typed graph/program codecs and bounded asset packages implemented; broader format capabilities remain planned
>
> Date: 2026-09-06

2026-09-08: authored soundtrack identity/gain/repeat now live in manifest version
2 when present; unbound projects retain version 1. Published music uses the
explicit `music-performance-v1` profile with program ABI 2 and unchanged 8 MiB
asset/16 MiB archive budgets. This avoids silently ignoring music in old readers.
See [work soundtrack validation](validation/work_soundtrack_2026-09-08.md) for
transaction, compatibility, Player ownership and remaining large-media limits.

## 1. Decision summary

Protobuf remains appropriate for typed graph/runtime records, but the current
`EffectPack` file must not remain the new editor's primary persistence model.
The new system uses three distinct artifacts:

1. **authoring project**: canonical editable graph, editor state and immutable
   content-addressed assets;
2. **published runtime package**: validated compiled program, required assets,
   shader products and product metadata;
3. **cache/index**: rebuildable compiled intermediates, thumbnails, database
   rows and search data.

Only the first two are durable data. A cache or catalog database is never a
source of truth.

## 2. Existing format audit

The current save path writes an effect directory containing at least:

```text
config.json
<effect-name>_cover.jpg
<effect-name>.skwp             # binary Protobuf EffectPack
project.rhythmgraph            # JSON graph + editor metadata + resource manifest
<external packaged resources>
SQLite catalog row
```

### 2.1 Existing strengths

- binary Protobuf is compact and portable for runtime data;
- `format_generation` and graph `format_version` reject unsupported formats;
- deleted Protobuf fields have begun to use `reserved` declarations;
- graph resource IDs are SHA-256 content identities and relative paths are
  validated;
- runtime physics state, native handles and most transient GPU objects are not
  persisted;
- graph and editor metadata are conceptually separated in
  `project.rhythmgraph`;
- `QSaveFile` makes individual pack, graph, cover and some resource writes
  atomic.

These practices should be retained in platform-neutral form.

### 2.2 Existing weaknesses

#### Multiple sources of truth

Name, ID, author, resolution and preview information are repeated across
`config.json`, `InfoComponent`, cover files and the SQLite catalog. Graph source
and compiled `EffectPack` can also disagree after a partial save or failed
catalog update.

#### Flat monolithic runtime schema

`EffectPack` owns one repeated root field for every component family. Adding a
render capability expands the root message and requires graph-to-EffectPack
adapter/importer work. This preserves a component editor internally even though
the UI has become a programmable graph.

#### Bulk data inside messages

Images, fonts, animation atlases and shader binaries appear as `bytes` in
several messages. Large blobs force whole-message allocation and rewriting,
prevent independent deduplication and make corruption or memory spikes affect
the complete package.

#### File-level atomicity is not package-level atomicity

Each `QSaveFile` commit is atomic, but the directory transaction is not. A
failure after writing metadata, resources or `.skwp` can leave a mixed package.
The catalog is updated after package files, so a valid package can exist without
an index row.

#### Weak evolution boundaries

A single global `format_generation` cannot express independent node, graph,
compiled-program and resource schema evolution. Several enums use a meaningful
value at numeric zero instead of an explicit `UNSPECIFIED` value, reducing the
ability to distinguish omission from an intentional selection.

#### Authoring/runtime duplication

The graph is serialized as JSON, then converted into a separate flat
`EffectPack`. Both contain structural and property information. Saving and
loading therefore rely on complex bidirectional adapters and round-trip tests.

## 3. Why Protobuf should remain

Protobuf is still a good fit for:

- strongly typed graph records and values;
- stable numeric field identities;
- compact compiled execution plans;
- cross-platform desktop/mobile packages;
- forward-compatible unknown fields;
- generated validation/migration tooling boundaries.

It is not a package container, asset store, transaction system, database or
human-readable manifest. Those responsibilities must not be forced into one
large message.

## 4. Proposed authoring project

An editable project is a directory with a stable extension, for example:

```text
Example.rhythmproj/
  CURRENT
  revisions/
    <revision-id>/
      manifest.json
      graph.pb
      editor.json
  assets/
    sha256/
      ab/<full-hash>.<ext>
  autosave/
  .cache/
```

The exact physical revision layout may be simplified after prototyping, but the
logical transaction rules below are mandatory.

### 4.1 `manifest.json`

The small human-readable manifest is the single source for package identity and
product metadata:

- format name and manifest version;
- project UUID and committed revision ID;
- stable machine-readable name/author fields and timestamps;
- optional localized display names, descriptions and author metadata keyed by
  BCP 47 locale, plus an explicit fallback locale;
- design profiles/canvas defaults;
- graph schema generation;
- required engine/runtime feature levels;
- asset records containing ID, hash, size, media type and logical role;
- optional published cover reference.

The manifest contains no binary asset payload and does not duplicate complete
graph properties. Localized display text is presentation metadata only:
project UUIDs, graph keys, feature IDs, asset IDs and runtime behavior remain
language-neutral.

### 4.2 `graph.pb`

`graph.pb` is the canonical authoring graph. It stores:

- stable document, node, port, edge and subgraph IDs;
- node `type_key` and per-node `schema_version`;
- strongly typed property values using `oneof`;
- port/property bindings and composite-node definitions;
- canvas and execution-relevant project settings.

It does not store ImGui state, absolute machine paths, runtime handles,
compiled shaders, GPU resources or current simulation state.

Node types remain extensible strings/hashed keys rather than one root enum.
Value categories use stable enums and `oneof`. Ports that are completely
defined by a registered schema should not be redundantly persisted unless they
are dynamic or needed to preserve an unknown node losslessly.

### 4.3 `editor.json`

Editor-only state changes frequently and is intentionally separate:

- node positions and sizes;
- groups, comments and colors;
- dock layout;
- expanded/collapsed state;
- selected viewer and pinned viewers;
- last navigation position.

Failure to parse editor state must not make the effect graph unplayable. It may
be reset independently and remains suitable for diagnostics and source-control
inspection.

### 4.4 Assets

Assets are immutable and addressed by SHA-256. Protobuf stores an `AssetId` and
metadata, never the full image/font/video payload. Identical content is written
once even when referenced by multiple nodes.

Logical identity and physical location are separate. A node references an asset
ID; an asset resolver selects the project file, installed shared cache or
packaged resource. Absolute paths are import-time information only.

### 4.5 Operator and content dependencies

Graph nodes store stable operator type IDs and schema versions. Semantic nodes,
presets and templates follow `builtin_nodes_and_presets_plan.md` and are not
implemented as hard-coded demo constructors.

Applying an ordinary parameter preset resolves its values into the project.
Reusable semantic components record their stable content ID/version and the
project-owned definition snapshot required for reproducible editing or an
explicitly declared package dependency with a compatible embedded fallback.
Official catalog updates never silently rewrite a committed graph. Updating a
linked component is an explicit validated project command; detaching it creates
a wholly project-owned subgraph.

User presets and templates are separate content packages under application
data. They can be exported/imported without changing the project format. The
published Player package contains the compiled operator plan and required
assets, not a dependency on the mutable Studio catalog.

## 5. Published runtime package

A published package is a single `.rhythmpack` archive or an equivalent signed
directory produced from a validated project:

```text
manifest.json
runtime/program.pb
assets/sha256/...
shaders/<profile>/...
preview/cover.webp
source/graph.pb              # optional authoring export
```

`program.pb` represents the compiled graph/runtime plan rather than a list of
legacy component families. It should contain compact operator IDs, value/state
slots, resource plans, render passes, observation points and diagnostic maps.
It is derived data and can be regenerated from a compatible source project.

Platform shader products are explicit package entries. No bgfx/native handles
are persisted. Runtime packages declare the engine ABI and feature set they
require; they do not rely only on a single application version number. The
same package format is consumed by Player on Windows, macOS, Android and iOS. A
package may contain backend/platform-specific shader products or texture
variants selected through the asset manifest, but it declares their target and
fallback policy. Studio validates Player compatibility for each requested
target before publishing.

## 6. Atomic save and recovery

Authoring saves follow a commit protocol:

1. collect an immutable project snapshot;
2. validate graph, references and schema versions;
3. write missing content-addressed assets first;
4. write graph, editor state and manifest into a new revision/staging location;
5. flush and reopen the written data for validation;
6. atomically replace `CURRENT` with the committed revision ID;
7. update the rebuildable SQLite/search index after commit;
8. garbage-collect abandoned staging data and old autosaves later.

The last valid `CURRENT` revision remains loadable after a crash at any earlier
step. Unreferenced assets and incomplete revisions are harmless and recoverable.

Published packages are built to a sibling temporary file, reopened, fully
validated and then atomically replace the destination. A failed export never
modifies the previous valid package.

Autosave uses separate revisions under application data or the project recovery
area. It never silently advances the user's committed `CURRENT` revision.

## 7. Protobuf schema rules

- use `proto3` with explicit presence where omission differs from the default;
- every enum begins with a domain-specific `*_UNSPECIFIED = 0` value;
- never reuse field numbers or enum numeric values;
- reserve removed field numbers and names;
- separate `manifest_version`, `graph_schema_version`, per-node schema version,
  `program_abi_version` and asset schema version;
- migrate persisted source data through deterministic version-to-version
  functions with golden tests;
- treat compiled programs and caches as replaceable when their ABI changes;
- bound message sizes, repeated counts, nesting depth and asset sizes before
  allocation;
- reject invalid UTF-8, unsafe paths, duplicate IDs and non-finite numeric
  values at the persistence boundary;
- do not use `google.protobuf.Any` as an unvalidated general property bag;
- do not hash raw Protobuf bytes as a universal semantic identity. Hash a
  defined canonical domain representation plus referenced asset hashes;
- use ordinary complete serialization APIs. Partial serialization is not a
  substitute for validation.

Unknown nodes should round-trip losslessly through a generic node record with a
stable type key and typed properties. They remain disabled with diagnostics
until their implementation is available.

## 8. Database and catalog policy

SQLite stores searchable/indexed projections such as project name, cover,
recent-open time and installation state. It must be possible to delete the
database and rebuild it by scanning project/package manifests.

A database failure after a project commit is reported as an indexing warning,
not as corruption of the successfully saved project. Database IDs do not become
project identities.

## 9. Current-data migration

The new application does not carry the old runtime model indefinitely. It
provides a tested one-time importer:

```text
project.rhythmgraph + .skwp + resources
                    |
                    v
new graph.pb + editor.json + content-addressed assets
```

Import preference:

1. use `project.rhythmgraph` as authoring source when valid;
2. use `.skwp` only to recover information absent from the graph;
3. reconcile duplicated metadata using explicit precedence and diagnostics;
4. hash and import external/embedded resources into the asset store;
5. compile and compare deterministic scene definitions and reference images;
6. save only the new format after successful conversion.

No Qt type or old `EffectPack` component type enters the new graph/runtime
public APIs. The importer is removable after the supported migration window.

## 10. Options considered

### One monolithic Protobuf file

Simple and compact, but rewrites large data, handles assets poorly, has no
package transaction, produces poor source-control diffs and repeats the current
monolith problem. Rejected for authoring projects.

### One SQLite project database

Provides excellent transactions and incremental updates, but is opaque to
inspection/source control, couples schema migration to storage tables and is a
poor distribution package. Useful for catalog/cache data, not selected as the
canonical authoring format.

### One ZIP archive for editing and distribution

Convenient to share, but frequent edits rewrite central-directory/package data
and complicate crash-safe incremental saves. Selected only for published
packages, not the live authoring workspace.

### JSON for the complete graph

Readable and diffable, but weaker typed evolution, larger parsing surface and
less suitable for compiled plans/mobile players. A deterministic JSON/text
export can be provided for review, while Protobuf remains canonical.

## 11. Validation gates before implementation

- define the first `GraphProject` and `CompiledProgram` schemas without legacy
  component-family root fields;
- round-trip unknown nodes and unknown fields;
- interrupt saves at each commit step and recover the previous revision;
- detect corrupted graph, manifest and asset hashes before runtime creation;
- import representative current projects and compare graph semantics;
- prove that deleting `.cache` and the catalog database loses no user data;
- load packages on the Null backend without SDL, Dear ImGui or a GPU;
- establish size/count limits and fuzz the persistence boundary.

## 12. Cluster persistence boundary

See `cluster_playback_plan.md`. Authoring data may contain role/layout presets,
parameter mappings, cues and authored playback/recovery policies. Published
packages declare runtime capabilities, quality variants and restoration modes.
They do not contain current peers, socket state, session tokens, private keys
or transient clock offsets. Network protocol versions are negotiated separately
from graph schema and runtime feature versions.

Players acquire validated content-addressed resources into OS application data
or cache locations, never the executable directory. Incomplete downloads cannot
replace the last valid package. Scene preparation and scheduled switching use
the existing package validation/transaction model, not a second cluster-only
effect serialization format.

## Current asset implementation (2026-09-07)

Project assets use `assets/sha256/<first-two-hex>/<full-hash>` without an extension.
Revision manifests contain SHA-256, byte size and media type; original file paths
are not saved. Import and publication run on bounded workers, and immutable
blobs remain after record removal so undo and prior revisions remain valid.
Garbage collection is not implemented. `LoadRevision` validates metadata only;
`Load` and `Save` also verify the project's asset bytes.

The first asset runtime profile is `texture-signal-assets-v1`: at most 64 assets,
8 MiB total asset bytes and 16 MiB ZIP archive size. Entries are `assets/<hash>`,
separate from Protobuf, and are validated in bounded memory without extracting
paths. Studio and CLI publishers share snapshot-to-package assembly. These
limits are an explicit initial profile, not the final large-video pipeline.

## Canvas evolution (2026-09-07)

Authoring schema 2 and compiled ABI 2 require width/height. Existing schema/ABI 1
is decoded with its original 640x360 behavior; new writes use version 2. The new
texture-signal-v2 runtime profile supports the canvas and the existing bounded
asset list, with manifest/program ABI and canvas identity checked together.
Dimensions are 16..4096, capped at 2,073,600 pixels in this initial profile.
Old packages remain fixtures; older Players reject the new explicit profile.
Preflight also limits total wire fields/messages before Protobuf allocation.
