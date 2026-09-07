# Coding, ownership and design rules

> Status: mandatory for project-owned implementation
>
> Date: 2026-09-06

## 1. Style baseline

Project-owned C++ follows Google naming and formatting conventions with one
intentional project override: indentation is four spaces rather than two.
`TabWidth` is four for editor display, but source indentation uses spaces and
tab characters are not committed.

The repository `.clang-format` and `.editorconfig` are authoritative for
mechanical formatting. The root `AGENTS.md` defines mandatory engineering
behavior for human and automated contributors.

Core naming examples:

```cpp
class RenderSession final {
public:
    void SubmitFrame(const FramePlan& frame_plan);

private:
    bool initialized_ = false;
    std::uint64_t frame_number_ = 0;
    std::unique_ptr<FrameScheduler> frame_scheduler_{};
};

constexpr std::uint32_t kMaximumViewerCount = 16;
```

All members have declaration-site defaults, declarations are not padded for
visual alignment, and a rename changes its declaration and uses in one
buildable change.

## 2. Ownership policy

The project does not use raw pointers as an ownership mechanism. It also does
not replace every object with a heap allocation. The selection order is:

1. value for ordinary ownership;
2. reference or `std::span` for synchronous borrowing;
3. stable value ID/handle for stored registry relationships;
4. `std::unique_ptr` for exclusive dynamic ownership;
5. `std::shared_ptr` only for demonstrably shared lifetime;
6. `std::weak_ptr` for an observer of shared ownership.

Project-owned APIs do not return registry object pointers or expose addresses as
identity. Renderer resources, graph objects, assets and physics bodies use
typed value handles whose validity is checked by their owning service.

Third-party and operating-system APIs sometimes require pointer syntax. Those
pointers are confined to adapters such as `platform_sdl`, `render_bgfx`, media
integration or the Box2D wrapper. They cannot cross into public RhythmRender,
graph, effect, UI or persistence contracts. A boundary adapter owns the RAII
translation and documents whether the external API owns, borrows or transfers
the object.

## 3. Composition rules

Classes are split by responsibility and lifetime, not by an arbitrary line
count. In particular:

- an application session coordinates services but does not implement their
  storage, validation or platform details;
- a render backend executes render contracts but does not own graph semantics;
- the graph compiler produces an immutable execution plan but does not render;
- UI panels issue application commands and display snapshots rather than
  mutating runtime internals;
- persistence validates and commits values without retaining live GPU, physics
  or UI objects;
- platform adapters translate native events without knowing graph node types.

Composition is preferred over inheritance. Interfaces are introduced at a real
backend, policy or test seam, not as one-method wrappers. Large coordinators are
kept thin by composing components that each own a coherent invariant and can be
tested independently.

## 4. Dependency and compilation discipline

Public headers expose the smallest stable project-owned contract. They avoid
third-party headers, concrete backend types and unnecessary implementation
details. Implementation-heavy classes use private implementation objects only
when that produces a meaningful dependency firewall.

Modules are divided so that changing a UI label, platform adapter or concrete
GPU backend does not rebuild the portable graph/runtime without a public API
change. Localization catalogs and data schemas are generated in focused targets
rather than collected into one global constants header.

Repeated strings and numeric protocol values follow these rules:

- stable cross-module identifiers live with the owning module's contract;
- display strings live in localization catalogs;
- closed numeric domains use scoped `enum class` values with explicit storage
  where persisted or exchanged;
- tunable numeric values live in focused configuration/schema definitions;
- implementation-only constants stay in the narrowest `.cpp` scope;
- a universal constants header is forbidden.

## 5. Review gates

Every project-owned change is reviewed for:

- explicit initialization and ownership;
- absence of raw owning pointers and direct `new`/`delete`;
- confinement of unavoidable external pointers;
- correct module dependency direction;
- class cohesion and testable composition;
- four-space formatting and naming conformance;
- an incremental build and focused tests for the affected target.

