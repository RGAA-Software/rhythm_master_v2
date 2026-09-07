# Displacement and temporal trails

The five-item continuation starts with reusable visual capabilities. This record
does not establish completion of the content library or Android acceptance.

## Displacement

`texture.displace` uses a source image and displacement map, with height-gradient
and centered RG-vector modes. Strength is signed in image-height units; map
sampling radius is in map pixels. Source edges mirror; map edges clamp. Transparent
vector maps are neutral. The renderer validates both handles, rejects destination
sampling and incompatible effect combinations, and keeps backend types private.

The focused shader adapts TiXL `Displace.hlsl` at revision
`fbc994d923e8a0142d2ff1b772e4d12248c5b0ba`. Exact bytes, MIT notice and adaptation
scope are retained in `provenance/tixl_effects.json`. No old-project or upstream
repository files are modified.

## Trails

`texture.trail` retains a per-channel peak envelope: fresh source highlights and
exponentially decayed history. Half-life is in seconds (zero bypasses and releases
history). Optional historical scale/rotation rates are per second. History uses
two RGBA16F targets, accounted at eight bytes per pixel within the existing
256 MiB budget. Unsupported target formats fail explicitly rather than silently
using a visually different lower-precision history.

Pause and repeated time preserve pixels. Reverse time or gaps exceeding 250 ms
seed a new history; reset-generation and viewport changes discard old resources.
This is bounded real-time playback, not arbitrary-time offline simulation.

TiXL `FeedbackAdjustImage.hlsl` and `AdvancedFeedback.cs` were inspected. Their
brightness/HSV/edge amplification and host graph are not the elapsed-time peak
envelope needed here; no code from those files is imported. The lifecycle uses
the project's existing render-target, graph and RAII contracts.

## Verification

- `out/displace-render-tests.log`: renderer contracts, GPU cases and shader
  compiler contracts passed. Seven actual D3D11 displacement cases cover bypass,
  both signs, gradient, rotation, mismatched source/map sizes and transparency.
- `out/displace-node-tests.log`: texture commands, runtime, content, template
  compatibility and source boundaries passed.
- `out/trail-render-tests.log`: render contracts and actual D3D11 captures passed;
  30/60 Hz one-second decay agrees within three 8-bit presentation levels, and
  ten-second decay becomes visually zero. Float target allocation/release is
  checked separately from display precision.
- `out/trail-node-tests.log`: pause, repeats, resizing, bypass, reset, resource
  bounds and graph cache passed, with existing template/content contracts.
- Android device `e2b3b128` reconnected. Actual Adreno 650 GLES tests now pass
  positive/negative vector displacement, RGBA16F history, 30/60 Hz decay, pause,
  reverse-time reset and device recreation. Six selected native suites pass;
  evidence: `out/android-effects-review/1915e3272030493fab2a55eb4541f02d`.
  This does not establish APK import, permissions, audio or lifecycle acceptance.

The extracted texture-program owner initially lacked `bx/platform.h`, so Android
selected embedded DXBC rather than GLES shader bytes. Native GPU execution exposed
this; the platform include is now explicit and both device cycles pass.

The Windows shader compiler emits failures to stdout; the Python adapter now
retains both output streams so shader failures include useful diagnostics.
