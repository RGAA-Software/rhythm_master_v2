# Authoring and visual-quality correction

The user reports that nodes are presented in an undifferentiated list and that
templates are too simple for the intended visual-authoring product. Both are
product gaps. Earlier user acceptance of dragging, styling and deployment did
not accept the whole editor or final visual content. Steps 1–7 remain incomplete.

## Immediate editor increment

The root graph and component workbench now share a node palette with collapsible,
localized functional categories and result counts. Search matches the displayed
name, stable type and category, reveals matching categories, and reports empty
results. The existing insertion/history path remains responsible for edits;
the workbench still excludes ancestors to prevent recursive component insertion.

This uses installed Dear ImGui's ImGuiTextFilter, CollapsingHeader and Selectable,
with project-owned category mapping and stable selection IDs. Reviewed source:
`third_party/sources/imgui/imgui_demo.cpp`, collapsing headers and text filtering;
upstream https://github.com/ocornut/imgui revision
`4806a1924ff6181180bf5e4b8b79ab4394118875`, MIT. No new third-party files imported
or dependency changes. Existing ImGui notices remain applicable. A foreign node
registry is unnecessary: the project's current operator descriptors already
provide the typed operations and component contracts needed for categorization.

This increment does not complete the full palette specification: the separate
effect library still needs category metadata, thumbnails, richer descriptions,
favorites, recent items and compatible-connection suggestions. Search currently
uses localized displayed text and stable IDs, not a fuzzy multilingual alias index.

Validation: Windows Studio and the two affected interaction targets compiled.
`out/node-palette-tests.log` records four passing suites: source boundaries,
Windows GPU smoke, component interactions and canvas interactions. The initial
post-link deployment failed because the user was running the deployed executable.
The user then explicitly authorized killing project processes that lock outputs;
the exact-path Studio process was terminated and deployment succeeded with all
16 DLLs. `out/node-palette-deploy-test.log` records the deployed smoke test passing
with a system-only PATH. These checks establish the integration baseline, not
final palette usability or visual-template acceptance.

## Content acceptance correction

Eight semantic components, 99 presets and 22 example projects describe inventory,
not accepted visual quality. The examples mostly establish isolated rendering
and graph capabilities. Do not expand the library by counting color variations,
renaming examples, or treating a successful playback smoke test as art direction.

Next visual work must deliver a small representative set before multiplying it:

- Audio-reactive layered motion with readable foreground/background separation,
  contrasting frequency responses and a deliberate silent-input appearance.
- Particle/feedback motion with coherent spatial composition and temporal detail.
- A three-dimensional composition with deliberate materials, lighting and camera
  motion, going beyond an isolated primitive used to verify rendering.

Reuse applicable open-source effects under the existing reuse policy. Record the
actual source, license, adaptations and missing runtime capabilities. Implement
needed visual primitives through shared modules rather than hardcoded template
branches. Communication and Apple work remain deferred.

For each proposed complete template, require a reproducible graph/package,
documented editable controls and audio inputs, representative captured frames
and a motion preview, declared output aspect ratios, and measured target-device
performance. Review composition, motion, visual distinction, parameter extremes
and silence behavior separately from functional tests. Capture the actual Player
output; do not substitute generated illustrations or promise TouchDesigner parity
on the basis of a screenshot. Android suitability requires device evidence.

The user subsequently raised the template target to at least 50 Basic and 50
Advanced templates. See `../template_quality_delivery_plan.md`. Meeting those
counts alone cannot close step 5; visual-template acceptance is still pending.
