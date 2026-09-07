# Initial semantic library integration

The Studio effect-component palette now loads eight data-authored entries using
the existing bounded project codec, graph compiler and component commands. It
supports Chinese/English title search, insertion into the current graph, exposed
parameters, internal editing, independent copies, undo/redo and standalone
publishing. Definitions/layouts are embedded in the project; Player has no catalog
lookup dependency. Existing modified definitions are not overwritten on insertion.

Each entry has Default and a curated variant. Preset decoding/application now
accepts the current embedded component definitions. `reset: true` restores that
component's full exposed defaults before applying overrides, preserving node
identity/extension data. An incompatible preset after an interface edit reports
an error and leaves current parameter values intact.

## Reuse decision

Read-only source review:

- cables `75d9960b75545cbd40037d4b48ccf52ac8c75b63`,
  `src/corelibs/subpatchop/subpatchop.js`: subpatch identity, dependencies and
  deletion/cache ownership. Its implementation requires JavaScript Port/Patch
  objects, browser GUI globals and mutable event callbacks.
- Material Maker `ad19fcf0ee34a7caf74df709dc4de7112f0d467d`,
  `material_maker/tools/library_manager/library.gd`: library indexing, localized
  display/filtering, read-only catalog ownership and preview metadata. Its loader
  uses Godot Node/FileAccess/ImageTexture and GDScript resource paths.

Those engine-bound modules are reference-only here; no code or images are imported
or translated. Existing first-party component expansion, registry, project codec,
history, preset decoder and ImGui controls supply the implementation. New code
adapts the official library to these contracts. Eight graphs are direct focused
extractions of existing project templates, not another effect implementation.

## Verification and limits

- `out/semantic-tests.log` and `out/semantic-presets-tests.log`: catalog validation,
  insertion without replacing the graph, atomic undo/redo, collision preservation,
  shared definitions and package/runtime roundtrips.
- `out/semantic-integration-tests.log`: application deployment/smoke, canvas and
  component interactions, content and template regressions. Each entry's default
  and variant also runs 60 Null-renderer frames; Default restores all exposed values.
- `out/semantic-player-*.log`: all eight initial default packages run 30 actual
  Windows GPU frames. This is playback evidence, not golden-image quality acceptance
  for every preset or viewport.
- `out/semantic-final-android-build.log`: latest shared content, preset support and
  native targets cross-build. The APK remains unchanged when only Studio catalog
  data changes; Android loads embedded compiled packages, not the Studio catalog.
  Device execution remains pending because ADB has no connected device.

Eight semantic nodes, 99 presets total (83 operator + 16 component), 22 templates.
The semantic entries are not counted again as complete templates. The final
40/120/24 targets, preview thumbnails, richer metadata/help, update migrations,
quality variants and broader visual acceptance remain outstanding.
See `content/semantic/README.md` for controls and current use.
