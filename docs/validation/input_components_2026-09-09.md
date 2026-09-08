# Input-processing semantic components — 2026-09-09

The official catalog now supports components that process the author's texture.
The preview harness can supply primitive input nodes, up to 32 root nodes and
128 edges. It requires exactly one root component directly feeding the texture
output and retains the existing compile/interface/preset/overall graph checks.
Insertion still copies definitions, internal layout and root parameters only.
It neither replaces the graph nor copies the demonstration input nodes.

The separate user component library's asset workflow is unchanged. Official
catalog-owned shader/model/image payload support is not delivered by this change.

## Flow glass

11 internal nodes use the already-attributed TiXL noise/displacement adapters,
existing blur/composite and first-party parameter/audio wiring. The four-node
preview harness compiles to 14 instructions. One required texture input and six
controls: flow, response, pattern scale, direction, softness and glow. Default
and Broad ripples presets change flow, refraction and softness. The component is
an editable processing chain; the stripes shown in the preview are a fixture,
not a new complete work. Provenance: `provenance/input_components.json`.

## Verification

- All 18 catalog entries pass insertion without demo nodes, connection to the
  existing graph, undo/redo, default/variant roundtrip, package and runtime tests.
- The actual component browser previews Flow glass before explicit insertion,
  inserts one node, supports undo, keeps window size stable and releases preview
  resources. The test can now explicitly target a catalog entry by content ID.
- Real decoded music/silence/low/high at the same four-second scene time:
  mean RGB differences 6.0495848, 27.8060590, 13.8658011, 17.6681811. Stable accounted
  texture bytes: 23,500,804 at the 1280×720 test capture size.
- The first 27-degree refraction default nearly canceled vertical displacement
  for the original gradient fixture and returned zero differences. A 70-degree
  default passed the unchanged 0.15 threshold, but the gradient obscured the
  refraction visually. The final fixture uses high-contrast stripes and 0.045
  idle refraction so the purpose is visible without playing music. Final decoded
  audio checks above pass on that final harness; early logs remain in `out`.
- Native USB Android GLES runs the final default harness and the variant
  inserted into the test's existing gradient graph, 60 frames each, nonblack
  output plus multipass/device recreation checks. This is a short functional
  check, not frame-rate or endurance acceptance.
- An actual D3D11 Player thumbnail is stored with package and pixel hashes.
  Desktop builds continue to deploy the complete 20-DLL Studio directory.

The real-PCM test's structural coverage check now also recognizes a displacement
filter with at least two audio bands. Exact expected instruction count and all
four decoded-audio pixel thresholds remain enforced; a filter component does not
need to include unrelated geometry to qualify for its own music test.

Logs: `out/r6-flow-glass-final-contracts.log`, `out/r6-flow-glass-music-final.log`,
`out/r6-flow-glass-browser-final-zh.log`, `out/r6-flow-glass-browser-final-en.log`,
`out/r6-flow-glass-final-android.log`,
`out/r6-flow-glass-final-variant-android.log`, `out/r6-flow-glass-thumbnail.log`,
`out/r6-flow-glass-deploy-build.log`.

Inventory: 18 semantic components, 36 semantic plus 132 native preset records,
42 complete-project examples. These are not 40 completed semantic components,
120 accepted independent visual presets or 50 Basic + 50 Advanced quality works.
