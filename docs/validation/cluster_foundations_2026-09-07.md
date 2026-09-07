# Cluster foundation validation

The foundation increment adds no room server or authenticated transport yet.
Apple is deferred; current evidence covers Windows MSVC and Android NDK arm64
on USB e2b3b128 (API 34). The complete builds pass 29 Windows suites and 21
Android native suites, including actual Android GLES tests inherited from the
Player baseline. APK installation/lifecycle acceptance remains pending.

Source extraction, worker limits and source hashes are recorded in
`provenance/gammaray_common.json`. Background project/asset operations now reuse
bounded workers. Tests verify draining, cancellation, reentrant capture cleanup,
worker-owned final release, errors followed by recovery, and file ownership on exit.

Clock/input contracts test one hour of virtual drift, epoch/replay rejection,
bounded delay filtering, monotonic slew, interpolation/hold/fade and fixed memory.
They do not measure physical screen synchronization or venue network capacity.

QR extraction uses the separately recorded first-party GammaRay wrapper and
unchanged MIT Project Nayuki implementation. See `provenance/gammaray_qr.json`
and `third_party/qrcodegen_source.json`. Unlike the original wrapper, output
always includes a four-module quiet zone, uses whole-pixel modules with at least
two pixels per module, and requests medium or higher error correction. Input
is limited to 1,024 bytes and requested image extent to 64..2,048 pixels.
The actual square extent rounds down to a whole module multiple.

`qr_contracts` verifies bounds, finder pattern, opaque black/white pixels,
uniform modules, quiet zone and binary-safe input. Four fixtures generated on
Windows and the Android USB device match byte-for-byte. The independent
test-only zxing-cpp 2.3.0 decoder passes 20 cases: original, reduced contrast,
90-degree rotation, integer 2x scaling, and a small central damaged region,
for short, medium, 1,024-byte and embedded-zero payloads. This is synthetic
image testing; camera focus, screen glare, real low brightness and expired
invitation handling are not covered by an image encoder.

Reproduce independent decoding with:

```powershell
out/qr-validation/Scripts/python.exe tools/test-qr-decoding.py out/windows/src/qr/fixtures --compare-fixtures out/android-qr-fixtures
```

The separate test environment installs `zxing-cpp==2.3.0`; it is not a Player
runtime dependency. Evidence: `out/qr-decoding.log`, `out/qr-build.log`,
`out/windows-build.log`, `out/android-device.log`. Device fixture directory:
`/data/local/tmp/rhythm-master-phase-a-20260906200553677/qr-fixtures`.
