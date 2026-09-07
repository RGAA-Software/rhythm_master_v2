# Localization and international text plan

> Status: required cross-platform product architecture; implementation has not started
>
> Date: 2026-09-06

## 1. Scope

Localization applies to both products:

- Studio on Windows and macOS;
- Player on Windows, macOS, Android and iOS.

The first complete locales are `zh-CN` and `en-US`. The architecture must allow
additional locales without changing graph schemas, project identity or runtime
behavior. Right-to-left and complex-script rendering are architectural
requirements even if a corresponding locale is not shipped in the first
release.

## 2. Identity and persistence rules

Persisted and programmatic identities are never translated:

- graph node type and instance IDs;
- port, property, command and setting keys;
- Protobuf field names and numeric enum values;
- asset IDs, package feature IDs and diagnostic codes;
- ImGui widget and docking IDs.

The presentation layer resolves those identities to localized titles,
descriptions, labels and messages. ImGui controls use stable hidden IDs where a
visible localized label would otherwise become widget identity.

Published packages may provide localized display metadata:

```text
default_locale: en-US
title[en-US]: ...
title[zh-CN]: ...
description[en-US]: ...
description[zh-CN]: ...
```

Missing localized metadata falls back to the package default and then to the
stable untranslated display value. Translation never changes graph execution.

## 3. Localization service boundary

Domain, graph, render and persistence modules emit stable message/diagnostic IDs
plus typed arguments. They do not format English sentences. A portable
`LocalizationService` owned by the application layer provides:

- BCP 47 locale parsing and normalization;
- operating-system locale discovery and explicit user override;
- deterministic locale fallback;
- named parameter substitution and plural selection;
- localized number, date, duration and byte-size formatting;
- catalog loading, validation and development-time missing-key reporting;
- notification when visible UI must rebuild after a locale change.

The formatting/CLDR implementation is selected by a focused dependency spike.
It remains behind the service interface so render, graph and Player code do not
depend directly on a localization library.

## 4. Catalog organization

Catalogs are split by product/module to avoid rebuilding unrelated C++ targets
for translation-only changes:

```text
locales/
  en-US/
    common.*
    studio.*
    graph.*
    diagnostics.*
    player.*
    legal.*
  zh-CN/
    ...
```

Catalog validation rejects duplicate IDs, invalid placeholders and mismatched
argument sets. CI reports missing and obsolete entries. English and Simplified
Chinese catalogs must both be complete for a release; fallback is a runtime
safety mechanism, not permission to ship incomplete primary locales.

## 5. Text rendering and fonts

Application strings are UTF-8. Filesystem paths stay in native path types and
are converted only at display or external-library boundaries.

Text rendering owns:

- per-locale font-family and fallback selection;
- dynamic or range-based glyph atlas management;
- high-DPI rasterization;
- complex shaping, combining marks and bidirectional layout;
- cursor/selection mapping for shaped text;
- missing-glyph diagnostics.

FreeType provides rasterization. Complex shaping and bidirectional behavior are
implemented behind the text service, with HarfBuzz and a Unicode bidi solution
as the expected direction. Basic Dear ImGui text rendering alone is not treated
as sufficient evidence of language support.

Bundled fonts require explicit desktop and mobile redistribution rights. The
release artifact records the actual font licenses it contains.

Current Windows implementation (2026-09-07): user selected the original system
Microsoft YaHei font. Noto bundling and 32-bit ImGui changes were rolled back.
Template-text validation must check expected words as well as valid UTF-8 and
available glyphs: incorrect GBK-decoded text can still satisfy both latter checks.

## 6. Input method requirements

Studio text controls on Windows and macOS must support:

- IME composition text and marked ranges;
- candidate-window placement near the active insertion point;
- selection, clipboard, undo/redo and keyboard navigation;
- filename, search, numeric expression and multiline property editing;
- focus transfer between graph, inspector, modal and native dialog;
- Unicode project names and paths.

Player input is smaller but uses the same UTF-8 and locale rules. Android and
iOS hosts translate native text/input events through the platform boundary and
must handle virtual keyboard, safe area and focus lifecycle correctly.

## 7. Layout and content rules

- layouts tolerate longer translations without fixed text-width assumptions;
- translated text is not manually truncated when wrapping or scrolling is more
  appropriate;
- icons do not replace text unless their meaning is established and accessible;
- color is not the only carrier of diagnostic meaning;
- screenshots and documentation are versioned per supported locale where used;
- user-visible errors contain a stable diagnostic code usable by support.

The language can be changed from settings. Most UI changes immediately; a font
atlas rebuild may occur at a safe frame boundary, but a process restart is not
the default design.

## 8. Testing and acceptance

Automated checks cover:

- catalog completeness and placeholder consistency;
- pseudolocalization with expanded strings;
- invalid UTF-8 and malformed external metadata;
- locale fallback and user override persistence;
- locale-independent graph/project serialization;
- screenshot tests for representative Studio and Player screens;
- number formatting without corrupting machine-readable numeric properties;
- language switching without changing ImGui/node/dock identity.

Manual platform gates cover:

- Simplified Chinese IME on Windows and macOS Studio;
- Retina/high-DPI glyph quality and mixed-DPI movement;
- Android/iOS virtual keyboard and lifecycle behavior;
- font fallback for project metadata and paths;
- clipping, overlap and navigation in both primary locales.

A platform is not declared localized merely because translated catalogs load.
It must pass text rendering, input, layout, persistence and recovery workflows
in each primary locale.

