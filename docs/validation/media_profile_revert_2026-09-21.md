# Media profile measured in the wrong checkout (2026-09-21)

## Failure and root cause

The first `tools/build-windows.py` delivery run for the Glazed Celestial Gate
template stopped at the Player deploy gate:
`Media dependency differs from the validated profile:
out/vcpkg-media-lgpl/x64-windows/bin/swscale-7.dll`.

Commit 0461be3 ("Re-validate Windows media profile on current host",
2026-09-17) had replaced every Release/Debug DLL hash, the configuration
strings and the source-archive hash in `provenance/media_lgpl_windows.json`.
Its configuration strings reference `D:/source/rhythm_master_v2/...`, a
sibling checkout that no longer exists on this host. The re-measurement was
taken against that checkout's rebuilt SDK and committed here; the hashes never
matched this checkout's binaries, so every subsequent delivery build in this
checkout was guaranteed to fail the deploy gate.

## Evidence for the revert

- All seven Release DLLs in `out/vcpkg-media-lgpl/x64-windows/bin` and all
  seven Debug DLLs in `debug/bin` (built 2026-09-07, before the 0461be3
  commit) match the pre-0461be3 profile hashes exactly.
- `out/release-sources/ffmpeg-vcpkg.zip` matches the pre-0461be3
  `source_archive_sha256` (e1859a9e...).
- The 0461be3 profile's own configuration strings name the v2 checkout as the
  measurement location.

The profile was therefore restored to the pre-0461be3 content
(`git checkout 0461be3~1 -- provenance/media_lgpl_windows.json`), which is the
profile validated by the evidence suites in
`validation/media_application_2026-09-07.md`. The host-migration paragraphs in
that document and in `validation/gpu_point_layered_volume_2026-09-17.md` were
corrected to state that the rebuild and re-validation happened in the v2
checkout, not here.

## Missed test path and permanent checks

- Missed path: the 0461be3 re-validation was committed without a subsequent
  delivery build in this checkout, so the hash mismatch was not caught until
  the next delivery run four days later. A delivery build must follow any
  provenance profile change in the same checkout before committing.
- Permanent check: the deploy hash gate itself is the guard and worked as
  designed; `tools/build-windows.py` remains the mandatory delivery path and
  is re-run after this revert.
