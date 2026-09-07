# Rhythm Master engineering rules

These rules apply to all project-owned source code, tests, build scripts and
tools in this repository. Imported third-party code is excluded unless its
ownership is explicitly transferred to the project.

## Open-source identity and third-party reuse

- Rhythm Master is an open-source project with commercial-grade quality goals.
  Do not infer a closed-source or paid product from that quality requirement.
- Apply `docs/third_party_reuse_policy.md` to third-party code and content.
- The user authorizes reuse of our own code in GammaRayPremium's px_common.
  Follow `docs/gammaray_common_reuse_plan.md`: extract focused modules, retain
  provenance, and distinguish first-party code from embedded third parties.
- Permissive and copyleft code, including GPL code from Coollab and ossia score,
  may be studied, reused and adapted when their actual terms and the combined
  artifact's licensing are compatible. Do not impose a blanket GPL ban.
- Record the source URL, revision, imported files, exact license/exception,
  modifications and target modules. Retain copyright and license notices.
- The repository's outbound license is not yet selected. Do not invent one or
  relicense third-party code. Before integrating code whose license determines
  distribution terms, resolve the license for the affected artifact.
- A wrapper, plugin boundary or dynamic library is not automatically an
  exemption from license obligations.
- Open-source reuse does not waive the zero-Qt or platform boundaries. Imported
  upstream sources retain their own style; project-owned adapters follow ours.
- Permission to maintain an imported fork does not transfer its copyright or
  remove its original license obligations.

## Formatting and naming

- Follow the Google C++ style as modified by this file and `.clang-format`.
- Indent with four spaces. Set the tab width to four and never commit tab
  characters in C++ source.
- Use `snake_case` for local variables, parameters and non-type identifiers.
- Use `snake_case_` with a trailing underscore for every non-static class data
  member.
- Use `kCamelCase` for constants and `CamelCase` for types and functions.
- Use one space between a type and its declarator. Do not align declarations
  with padding spaces.
- Initialize every data member at its declaration. Constructors may replace the
  default through their initializer list but must not be the only initialization
  path.
- Format only files changed for the current task; do not reformat imported or
  unrelated code mechanically.

## Ownership and lifetime

- Raw owning pointers and direct `new`/`delete` are forbidden.
- Prefer values and references when allocation or shared lifetime is not
  required. Do not introduce a smart pointer merely to avoid value semantics.
- Use `std::unique_ptr` for exclusive heap ownership.
- Use `std::shared_ptr` only when ownership is genuinely shared, and
  `std::weak_ptr` for stored observers of that shared lifetime.
- Project-owned public APIs and stored project members must not expose raw
  pointers. Use references, values, spans, stable value handles or explicit
  ownership types.
- A short-lived, checked borrowed pointer returned by an external API may exist
  only inside the synchronous boundary call that requires it.
- Raw pointers required by SDL, bgfx, native platform APIs, media libraries,
  Box2D or another C API must remain inside the narrowest project-owned adapter.
  Document the ownership rule, initialize stored boundary exceptions to
  `nullptr`, and never propagate them into graph, effect, UI or persistence
  APIs.
- Wrap non-pointer native handles in move-only RAII values where ownership is
  involved. Public render APIs expose project handles, never bgfx/native types.
- Use deterministic RAII for files, mappings, threads, GPU resources and other
  acquired resources. Manual paired cleanup spread across call sites is not
  acceptable.

## Architecture and design

- Prefer direct reuse, focused extraction and adaptation of mature open-source
  implementations over writing equivalent functionality from scratch. Inspect
  relevant upstream code first; record reuse decisions and concrete compatibility
  gaps under `docs/third_party_reuse_policy.md`. Keep project code focused on its
  contracts, ownership, platform adapters and product behavior.
- Prefer `C:/source/vcpkg` for all third-party dependencies, including build-time
  tools. Reuse compatible installed target-triplet packages first; use vcpkg
  ports/features for missing packages before considering a separate source build.
  Do not build FFmpeg through a project-owned source-build script.
  Check version, features, ABI, license and platform compatibility before adoption.
  Record concrete gaps and any retained source/fork exceptions in
  `docs/technology_stack_evaluation.md`; do not silently replace a validated fork
  or upgrade the shared vcpkg installation and unrelated packages.
- Consult `docs/technology_stack_evaluation.md` for confirmed versus pending
  library choices. Do not turn a candidate or an untested platform claim into
  a frozen dependency without recording the required validation.
- Cluster services follow `docs/cluster_playback_plan.md`: local rendering,
  bounded asynchronous transport and typed input snapshots. Do not introduce
  networking dependencies into the standalone render core or expose socket,
  Asio, cpr or QUIC types through graph/runtime public APIs.
- Godot is the primary design and source reference for 3D capabilities; follow
  `docs/godot_3d_reference_plan.md`. Evaluate focused source reuse, retain
  notices, and adapt dependencies to our contracts. This does not authorize
  embedding the whole engine or leaking Godot types into project public APIs.
- FFmpeg is the sole media demux/decode/encode/mux backend for the new project.
  Follow `docs/media_pipeline_plan.md`. Do not introduce VLC/libVLC, Qt
  Multimedia or a second high-level player/decoder as a fallback.
- Audio device output/capture, permissions and presentation remain narrow host
  adapters; they do not own a competing playlist, playback clock or decode path.
- Keep FFmpeg types and native resources private to RAII media adapters. Graph,
  audio-feature and render public APIs use project frame/block types and handles.

- Apply single-responsibility design and prefer composition/delegation over
  inheritance and coordinator growth.
- Separate platform hosting, UI, graph domain, compilation, runtime execution,
  rendering, persistence, assets and release services behind focused contracts.
- Do not create giant application, renderer, editor or session classes. Extract
  a component when it owns a distinct invariant, lifecycle or reason to change.
- Do not create artificial one-method wrappers or inheritance hierarchies only
  to reduce line count. A component must own a meaningful responsibility.
- Keep dependency direction inward: domain and runtime code cannot depend on
  SDL, Dear ImGui, platform headers or concrete bgfx types.
- Use stable value IDs/handles across registries and module boundaries instead
  of borrowed object addresses.
- Make ownership, thread affinity and mutation points explicit in public
  contracts. Background work publishes immutable/value results and never
  mutates UI or render state directly.
- Third-party types are private implementation details. Changes to a backend or
  platform adapter must not force graph/runtime consumers to rebuild without an
  actual public-contract change.

## Verification

- Specify `encoding="utf-8"` for Python reads/writes of project text. Never
  rewrite UTF-8 content through the Windows default code page. Text validation
  must check expected content as well as valid encoding and glyph availability.
- Use Python for new project tooling, including deployment, packaging and verification. Keep CMake
  responsible for build configuration and invoking those scripts, not procedural
  deployment logic.
- Rename declarations and all uses atomically within a buildable module.
- Build and test the affected target before moving to the next module.
- Use incremental builds by default. Do not clear caches or build directories
  unless the user explicitly requests it or a diagnosed build-graph fault
  requires it.
- Use 20 build workers by default; preserve configured incremental caches.
- The user authorizes directly terminating this project's Studio/Player processes
  when they lock build or deployment outputs. Verify each process executable path
  belongs to this project's output directory, terminate it, and continue without
  requesting confirmation. This does not authorize stopping unrelated processes.
- Treat compiler warnings in newly changed project code as defects.
