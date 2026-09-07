# File-backed audio ranges: large-media prerequisite

The shared storage/media adapters now support reading a validated range of an
open local file without allocating its entire contents. This is a prerequisite
for large music packages; **the published package and authoring limits have not
yet changed** in this adapter step.

## Contracts and reuse

`storage::FileBytes` owns a shared open-file lease with value ranges and explicit
size budgets. Reads serialize the native cursor and write into caller-owned spans;
there is no whole-source buffer, mapped-file dependency or exposed native handle.
Copies and nested ranges keep the original file alive. Out-of-range requests fail,
range EOF returns zero, and unexpected underlying truncation fails. I/O belongs
on workers. Callers must publish immutable content through atomic replacement;
an arbitrary in-place modification is not an immutable snapshot, particularly on
POSIX hosts. Windows readers deny write sharing while permitting delete sharing.

The POSIX ownership and seek/tell boundary adapts first-party
`GammaRayPremium/src/px_deps/px_common/file.cpp` under the user's explicit reuse
authorization. Original source remains unchanged. Windows uses the existing
project `atomic_storage` HANDLE pattern. See [provenance](../../provenance/file_bytes.json).
No new library, FFmpeg rebuild or outbound license selection is involved.

`AudioDecoder` retains this value source across exact seek/reopen and uses the
existing FFmpeg custom AVIO callbacks (32 KiB buffer, demuxer whitelist, no nested
path/network opening). Small immutable-byte and ordinary-path constructors remain
available. File range size is bounded by the caller's validated source contract;
the small embedded-byte overload retains its existing 16 MiB limit.

## Windows replacement finding

The new regression exposed `MoveFileEx` returning access denied when replacing a
file held by an active reader, even with delete sharing. The existing replacement
adapter now tries `FileRenameInfoEx` with replace/POSIX flags only after that
failure. Microsoft specifies that old handles remain valid and subsequent opens
resolve the replacement in [FileRenameInformationEx](https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-fscc/4217551b-d2c0-42cb-9dc1-69a716cf6d0c).
The staged source is flushed first; failure preserves existing files. There is no
delete-then-move gap. Older/unsupported filesystems can still report failure;
power-loss directory durability is not newly claimed for Windows.

## Evidence

- `file_bytes_tests`: 32 MiB file, nested EOF/bounds, retained source after parent
  release, two simultaneous readers doing 1,000 reads each, atomic replacement
  while old readers remain valid and failed replacement preservation.
- `media_audio_tests`: a WAV placed after more than 16 MiB of unrelated container
  bytes decodes exactly like the original file, including final partial block,
  exact sample seek and canceled seek retaining the previous decoder generation.
- Windows storage/media/source checks passed. The initial CRT and simple Windows
  sharing attempts failed the actual replacement test; the final POSIX rename
  branch passes it. Existing no-overwrite publication remains separate.
- Incremental Windows all-target build/deploy completed. Fourteen affected tests
  passed, including asset/persistence/package loading/import, AAC/H.264 encoding,
  soundtrack Player, actual GPU export and export jobs. Evidence:
  `out/file-audio-integration-tests.log`. Formatting and source-boundary checks
  passed. Android native Player and the verified companion relink APK rebuilt.
- USB phone `e2b3b128`, `/data/local/tmp/rhythm-file-audio/`: range, media and
  publication executables passed. Logs: `out/file-bytes-android-tests.log`,
  `out/file-audio-android-tests.log`, `out/file-publication-android-tests.log`.

Next: reuse the already pinned miniz callback reader/writer and stored-entry
iterator for a versioned large-music package. Keep metadata and ordinary assets
under their existing budgets, validate large music incrementally before commit,
then pass the retained file range through prepared assets and Player audio.
Do not treat this prerequisite as completion of large-song authoring, package
import, Android APK lifecycle, arrangement or the 50 + 50 content goal.
