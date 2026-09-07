# Runtime package and Player increment

Windows Studio publishes a compiled ABI 2 program and manifest into a bounded
standard ZIP `.rhythmpack`. The Player links the portable runtime/package code,
without Studio, node-editor or authoring history. Python's independent ZIP reader
confirmed entry names, CRC and program SHA-256 for the generated builtin package.

Native contracts cover deterministic program round trips, malformed slots,
property types/limits, ABI rejection, ZIP size/path/hash restrictions and atomic
publication/installation. Failed imports preserve the previous installed file;
failed session loads preserve the active package. Async publication captures a
value snapshot, rejects overlapping jobs and drains on destruction.

Windows Player is deployed automatically with DLLs and resources at
`out/windows/src/windows_player/deploy/rhythm_player.exe`. It supports package
path loading, pause/resume and restart; its GPU smoke test completes 30 frames.
The shared session independently tests paused feedback, no background time
catch-up, resize, resource invalidation and replacement devices.

Android debug APK builds with SDL 3.2.20 Java/native hosting, project-owned Java
controls and document import, and the same private bgfx backend using ESSL.
Python verifies v2/v3 APK signatures, ZIP/ELF 16 KiB alignment, every native
dependency and absence of Studio/editor/Qt code markers. These archive checks
do not establish App lifecycle or touch acceptance.

Device: USB `e2b3b128`, model 22021211RC, Android API 34, arm64-v8a.
APK installation was rejected by the device with
`INSTALL_FAILED_USER_RESTRICTED: Install canceled by user`. User action on the
phone is pending. The task remains active; no Android completion is claimed.

An independent native EGL pbuffer probe runs on this device's Adreno 650 GPU
without requiring APK installation. It exercises the actual project bgfx
adapter. Its first run failed: logical top red appeared blue (top R/B=8/247,
bottom=247/8). The adapter now projects GLES render targets with the orientation
needed to keep the public top-left UV contract and adjusts target scissors.
Retesting returned top=247/8 and bottom=8/247, including two consecutive device
creation/destruction cycles. This proves offscreen GPU behavior only, not
SurfaceView lifecycle, physical display orientation or presentation latency.

Evidence: `out/windows-build.log`, `out/android-device.log`,
`out/android-gpu-build.log`, `out/android-arm64/apk/verification.json`.

Current writes use texture-signal-v2; legacy texture-signal-v1 and texture-signal-assets-v1 remain readable. Media decoding, authenticated package
acquisition, cluster join, scene/point domains and production distribution remain
separate planned work. Android install acceptance will not close those gates.

Asset profile validation: 64 records / 8 MiB aggregate uncompressed asset budget,
SHA-256 and size matching, exact manifest/archive correspondence, no filesystem
extraction. Windows 21 tests and Android 13 native suites pass. Independent
Python verifies the CLI-published asset ZIP, and the same package executes
60 GLES frames on the USB device (out/android-assets-gpu.log). This validates
asset transport/retention, not image/video decode or presentation of those assets.

Canvas/ABI 2 validation: the frozen legacy-v1.rhythmpack fixture remains readable.
New schema/ABI 2 requires validated canvas dimensions and package metadata parity.
Windows 23 suites pass. A 720x1280 Windows package containing one immutable
asset completes 60 actual Adreno 650 frames. Aspect-fit math preserves portrait
and square proportions within bounded previews. This does not establish Android
SurfaceView touch/lifecycle acceptance while APK installation remains pending.

Template acceptance increment: 24 Windows suites and 16 Android native suites
pass, including source template/profile coverage and runtime round trips. Both
new Windows-published templates (portrait noise and square curve pulse) produce
visible output over 60 actual Android GLES frames; Windows Player also completes
30 GPU frames for each. The generic template probe checks nonblank output, while
the original fixture retains its stricter channel/orientation assertions.

Foundation/input increment: 28 Windows suites and 20 Android native suites pass.
Tests cover one-hour virtual clock drift, bounded stale-input interpolation/fade,
session/role/control operator cache invalidation, and bounded asynchronous I/O
worker shutdown/reuse. Source provenance: provenance/gammaray_common.json.
Device evidence: /data/local/tmp/rhythm-master-phase-a-20260906200008828.
No authenticated network transport or physical display synchrony is established.
