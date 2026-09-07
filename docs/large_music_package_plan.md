# Large music package implementation slice

Status: music-performance-v2 is implemented and validated through Windows Studio,
Windows Player and Android native PCM/GLES. Actual Android APK installation and
lifecycle acceptance remain blocked by the phone's USB installation restriction.
See [validation](validation/large_music_packages_2026-09-08.md).
Existing music-performance-v1 remains readable and retains its original limits.

## Container and ownership

- Reuse the pinned MIT miniz implementation and its read/write callbacks. Keep
  `.rhythmpack` as ZIP, program ABI 2 and existing graph/parameter contracts.
- Introduce an explicit `music-performance-v2` profile for one file-backed music
  attachment. Metadata and ordinary assets keep their existing limits; one
  `media/<sha256>` stored ZIP entry can hold up to 256 MiB of encoded music.
  Bound the whole archive separately to 272 MiB.
- Music uses ZIP STORE because FFmpeg must seek inside the encoded stream.
  Reuse miniz's stored-entry iterator to validate local-header offsets, range and
  CRC, and the existing SHA-256 implementation to validate content in chunks.
  Do not hand-write a ZIP parser, inflate whole songs or add a second decoder.
- Bound miniz's native allocations as well as entry sizes. A malicious central
  directory must not allocate memory proportional to the whole music allowance.
- Ordinary in-memory `EncodePackage`/`DecodePackage` preserve the small profile.
  Large-package publication/import uses file APIs on bounded workers, validates
  before atomic commit, and retains the opened music range across decoder seeks
  and package replacement. Do not expose miniz/FILE/native types outside adapters.
- A stream attachment is an explicit audio record, not a way to bypass texture,
  image/video, geometry or ordinary asset budgets. Validate the soundtrack binding
  against that record and reject unrelated or unlisted large entries.

## Integration order

1. Bounded file/range and existing FFmpeg I/O adapter (validated; see
   [evidence](validation/file_audio_ranges_2026-09-08.md)).
2. File ZIP reader/writer, allocation/entry limits, checksum and cancellation
   tests; then versioned manifest and atomic package publication.
3. Prepared soundtrack and Player's existing FilePlayback worker retain the
   file-range source. No UI/device callback reads files.
4. Studio imports/binds a larger song with the same revision/cancel guard and
   asset identity; saves/reopens/publishes it. Ordinary assets keep their budget.
5. Actual Windows Studio/Player and USB Android PCM/GLES validation from the
   published work, memory bounds and retained old reader during replacement.
6. Android APK picker/import/audio/lifecycle acceptance once phone USB installation
   is permitted. Native executables do not replace this acceptance step.

This slice does not claim multitrack arrangement, instant exact seeking in every
codec, gapless loops, a 50 + 50 finished template library or completed Android
application acceptance. Those remain on the authorized main roadmap.
