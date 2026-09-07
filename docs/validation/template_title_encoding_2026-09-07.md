# Template-name mojibake correction and font restoration, 2026-09-07

The reported name was wrong in `content/templates/prismatic_lotus/manifest.json`:
`妫遍暅鏄熻幉` instead of `棱镜星莲`. The local authoring generator had been read
using Python's default Windows GBK encoding and then rewritten as UTF-8. Reversing
that exact GBK/UTF-8 transformation recovers the intended four-character title.
The corrupt string is valid UTF-8 and its glyphs exist in both fonts, so the
previous glyph-coverage test and hardcoded font sample did not detect this bug.
This was an authoring-data error, not evidence that Microsoft YaHei lacked these
characters. Inspection of the other template titles found no equivalent reversible
GBK corruption. No heuristic bulk conversion of unrelated text was performed.

Per user instruction, the Windows host again loads local Microsoft YaHei
(`C:/Windows/Fonts/msyh.ttc`, 18 px, ChineseFull) with its original fallback.
The temporary Noto-loading code, 32-bit ImGui ABI change, font staging target and
font-deployment copy calls were rolled back. The imported font/notice files remain
an unused reference; older local output folders may retain the inactive asset.
No Windows system font was copied or modified.

## Correction and prevention

- Corrected the template's Chinese catalog label and project title. The authoring
  script `out/author-prismatic.py` now uses explicit UTF-8 text I/O and the correct
  literal. AGENTS.md records the explicit-encoding rule for future tools/edits.
- Existing template contracts now assert the actual Chinese label and title, and
  their preservation through package encoding/decoding. Encoding validity alone
  is not sufficient to establish correct text.
- `template_text_gpu_tests` reads the compiled catalog and template through the
  same project APIs as Studio, verifies their expected content, and renders those
  actual values with the original host font. Screenshot:
  `out/windows/template-text-review/template-title.png`. This replaces the
  insufficient hardcoded demonstration of a font's glyphs.
- Corrected the first-party review project's title in a new revision, retaining
  its original revision and preserving graph/editor bytes. A scan of project
  CURRENT entries in this workspace found this one affected saved review project.
  No unrelated user project metadata was rewritten.
- Rebuilt/redeployed both applications and packages incrementally with 20 workers.
  `out/title-encoding-build.log` records the build; `out/title-encoding-tests.log`
  contains ten passing suites: source boundaries, templates, actual text GPU,
  deployment/Studio/Player smoke and four editor interaction suites.
- The corrected saved project also opened with seven of seven previews and
  rendered 30 frames (`out/title-project-smoke.log`). Reopening/publishing it
  produces the same bytes as the corrected built-in template package.

Final package SHA-256:
`d42e851d045201e233d7bcd00714b7dd80b0a98c0ed4c6c4728704ddda69111e`.
Fresh synthetic and silent motion captures were generated from that exact package:
`out/lotus-title-preview/daf3ff1571c048a58f6ca7fe84b7eb00` and
`out/lotus-title-silent-preview/aea30ed0c0404732bd001268da084cb0`.
The review page's packages, videos and hash records were refreshed. Only metadata
changed in the effect; graph/runtime program bytes are unchanged.

`cjk_font_2026-09-07.md` records the superseded font experiment and must not be
reported as the current font configuration or as having fixed this metadata bug.
