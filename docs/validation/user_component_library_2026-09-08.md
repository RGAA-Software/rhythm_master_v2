# Cross-project user component library, 2026-09-08

Studio's inspector now has **My component library / 我的组件库**. Select a
component instance, save it, then add the saved component to any project.
The library path is displayed; a `.rhythmcomponent` folder can also be copied
and loaded explicitly on another machine. Insertions are ordinary undoable edits.

Capture retains only the selected instance, its nested definition closure,
public parameter values, internal layouts and referenced immutable assets.
Definitions receive content-derived names, avoiding collisions with unrelated
projects' local `component.user.N` types. Repeated captures/insertions reuse the
same definitions; edited conflicting definitions are not overwritten. Codec
extension records are normalized without discarding unknown fields.

One bounded worker handles scanning, saving, validation and asset copying.
Workers publish value results with the original document identity/revision;
new edits or drafts reject stale insertions. Saving drains on shutdown.
The library scans at most 1024 entries and offers at most 256 components per
directory; damaged entries are reported and skipped. The existing graph,
component, asset and package limits still apply.

## Reuse and verification

This feature reuses the project's DetachComponent closure walk, AddSemantic
insertion, Protobuf/layout codecs, transactional project storage, immutable
asset copy/verification and the GammaRay-derived bounded executor. No new
third-party source, dependency or persistence format was introduced. A component
folder is an ordinary versioned project snapshot specialized to one root
component, so nested scalar/point/scene components are not forced into a texture
preview harness merely to save them.

- `out/component-library-tests.log`: capture/reload of nested controls/layout,
  content naming, conflicting local types, referenced PNG travel, insertion,
  undo, bounded async operation and expected-revision metadata pass.
- `out/component-library-ui-tests.log` and
  `out/component-library-ui-chinese-tests.log`: English and Chinese real ImGui
  activation tests open the panel, save, observe the managed entry and request
  an asynchronous insertion. Fresh and existing directories were tested.
  UI identities use stable component filenames rather than path separators.
- `out/component-library-android-tests.log`: the same core library/persistence
  contracts pass on USB Android e2b3b128. This validates portable authoring data;
  it does not add Studio editing to Android Player. The latest texture-lifetime
  observation regression and GLES contracts pass there as well.
- Studio was incrementally rebuilt and its complete Release deploy refreshed.
  Source boundaries and deployed startup with music pass.

This completes this cross-project library increment. Internal component viewers
and single-instance unpacking were subsequently added; see
`component_previews_2026-09-08.md` and `component_unpack_2026-09-08.md`.
Automatic official-component migration, media export and the rest
of the visual-authoring/content roadmap remain separate work.
