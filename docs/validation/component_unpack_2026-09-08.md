# Unpack one selected component instance

The component panel now offers **展开所选实例（一级） / Unpack selected instance
(one level)** when one component node is selected. It replaces only that
instance with its immediate body. Nested component nodes remain components;
other instances and embedded definitions remain available for reuse. The older
whole-graph expansion remains a separate command.

The pure `UnpackComponent` command reuses the existing component expander for
validation and its public-parameter substitution rules. No third-party source
or dependencies are added. It retains the selected instance ID at its output,
remaps incoming wires and named bindings, removes overridden internal defaults,
renames local signals to avoid parent-scope collisions and translates internal
layout relative to the old output position. Instance parameter overrides become
body properties. Nested component overrides continue through normal expansion.

History applies the result as one revision-checked undo step. Generated node
IDs use the caller's reserved range and are not reused after undo. Original
snapshots, referenced assets and shared component definitions are preserved.

## Evidence

- `out/component-unpack-tests.log`: isolated instance changes, parameter and
  named input transfer, retained nested instances, collision-safe local signals,
  external-default replacement and undo/ID behavior pass.
- `out/component-unpack-integration-tests.log`: the 197-instruction Resonance Live
  performance is edited, saved, published and then unpacked. Its root changes
  from nine to 35 nodes while the field stays a reusable component. Four actual
  D3D11 captures driven by FFmpeg-decoded demo/silence/low/high PCM are **pixel
  identical** before and after unpacking. All seven selected/fixture tests pass.
- `out/component-unpack-delivery-tests.log`: real ImGui activation reaches the
  command, source boundaries and deployed Studio/Player startup pass.
- `out/component-unpack-android-tests.log`: the same command and full authoring
  API workflow pass on USB Android e2b3b128. This validates portable data/core
  behavior; Android Player does not gain a Studio editing UI.

Both Windows Release executables have refreshed sibling `deploy` bundles with
their DLL/resource closure. Automatic component migration and media export are
still separate work; this does not complete the entire authoring roadmap.
