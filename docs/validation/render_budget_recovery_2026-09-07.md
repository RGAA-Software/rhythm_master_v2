# Render budget recovery — 2026-09-07

## Behavior and boundaries

Studio and Player now use `Runtime::EvaluateSafely`. Resource admission failures
return a typed budget result with no output handles, release partial runtime
resources through existing RAII owners, and latch the rejected plan. Changing
nodes (even without a revision change), canvas extent, reset generation or
prepared resources permits a retry. Time and audio updates alone do not retry.
The raw `Evaluate` contract still throws; invalid inputs and unrelated backend
faults are not swallowed. Successful frames do not compare or copy the plan.

Studio keeps the graph editable and shows a localized output message and retry
button. Failed node preview capture disables previews; they can be re-enabled.
Template thumbnails fall back to selection buttons when allocation fails, and
failed live previews can be retried. Windows Player shows the diagnostic;
Android Player publishes a status code and presents an empty background while
keeping its host loop alive. A previous final image is not retained after failure.

The existing 256 MiB cumulative texture limit, 1024 logical texture slots and
240 effect passes remain unchanged. Presentation may use the remaining 16 bgfx
views, so effect pass exhaustion does not consume the host's UI reserve. Null
and GPU backends enforce the same pass policy. The imported bgfx configuration
also has a 128-framebuffer handle pool; invalid allocation handles now report
`kBackendResources`. This is separate from malformed sizes, stale handles and
thread affinity errors. Deferred bgfx destruction is exercised through EndFrame.
No dependency or imported source was added; existing resource validation and
bgfx RAII ownership were extended at the project adapter boundary.

This is recoverable admission, not transient texture reuse, automatic quality
selection, a larger resource budget, or a guarantee that arbitrary graphs fit.
Other previews releasing memory does not automatically restart a rejected graph;
use retry, reset or change the scene. Android still uses the existing status-code
presentation; its friendly localized native error panel remains future work.

## Verification

- Null renderer/runtime: 80 texture effects at 1024 square exhaust bytes; 260
  tiny effects exhaust passes. Repeated time updates submit no graph work and
  retain no partial resources. Editing the plan without changing its revision
  recovers output. Host presentation remains possible after each failure.
- Real Windows D3D11 Studio: Resonance Gate at 1024 square triggers the limit.
  The editor continues for 240 frames, including 121 limited frames. The test
  saves a 640x360 revision and activates the actual Reopen button; it observes
  89 recovered frames in the final 90-frame measurement interval.
- Real GPU: 260 tiny effect targets exercise bgfx handle exhaustion. A separate
  single-target sequence exhausts 240 passes and still submits actual ImGui
  geometry to the backbuffer. The two failures are asserted independently.
- Final Windows delivery: 9/9 selected checks pass, including music PCM/GPU,
  catalog popup, both deployed app startup checks and source boundaries. Android Release APK
  and GLES probe compile incrementally; the final native GLES pixel/device-recreation
  suite also passes on USB device e2b3b128. APK installation is not claimed.

Evidence: `out/budget-gpu-test.log`, `out/budget-app-tests.log`,
`out/budget-delivery-tests.log`, `out/budget-android-final-build.log`,
`out/budget-android-final-gpu.log`. The GPU regression is CTest `budget_gpu`;
its saved projects stay under the build directory, never the user's project.
