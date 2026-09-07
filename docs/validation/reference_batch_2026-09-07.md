# First six visual references and catalog

These are three Basic and three Advanced authored candidates. Automated behavior
checks and developer image inspection are recorded separately from user visual
acceptance. They do not complete the 50 Basic / 50 Advanced target.

| Tier | Template | Composition |
| --- | --- | --- |
| Basic | Layered neon | Layered audio ring, glow and dust |
| Basic | Aurora clouds | Two evolving fields, signed displacement and color layers |
| Basic | Firefly garden | Warm fine particles, soft jade lights and short trails |
| Advanced | Prismatic lotus | Polar depth, mirrored contours and two glow scales |
| Advanced | Stellar currents | Independent cyan/gold curl fields and transformed temporal trails |
| Advanced | Orbital reliquary | Six inclined metal tori, central blue sphere, gold satellites and two lights |

The tori adapt Godot 4.5.1's primitive mesh sampling, seams and normals, with our
counterclockwise index convention. GLM's installed Perlin implementation supplies
the potential for a bounded 33x33 curl-velocity lattice, refreshed at 30 Hz fixed
simulation time. Neither Godot nor GLM types enter public graph/runtime contracts.
Records: `provenance/godot_3d.json`, `provenance/glm.json`.

The three new effects are also exposed as editable semantic components with a
default and a purposeful motion/material variant. Current authored inventory is
28 runnable examples, 11 semantic components and 126 preset records. These are
inventory counts, not counts of visually accepted templates or presets.

## Browser

Studio now filters by Basic/Advanced/Example, subject and localized text. The
separate semantic palette groups components by subject. All 28 examples have
256x144 thumbnails captured from the actual D3D11 Player; source graphs remain
the editable originals. A selected template plays inside the browser using one
bounded PackageLoader and one host-thread Session. Applying remains a separate
undoable project command. No separate renderer or graph implementation is used.

`tools/render-catalog-thumbnails.py` reproduces thumbnails with package/pixel
hashes and explicit synthetic input metadata. `thumbnail.json` travels beside
the raw RGBA file. `catalog_gpu_tests` opens the actual ImGui browser, clicks a
thumbnail and verifies ongoing animated rendering without applying the project.
Evidence: `out/catalog-complete-tests.log`, `out/windows/catalog-review/catalog.tga`.

## Actual Android measurements

Device: USB `e2b3b128`, model 22021211RC, Android API 34, Adreno 650. The first
Debug measurements exposed excessive CPU cost for unoptimized GLM particle
fields. A separate Release build preserves the Debug cache. The table is actual
GLES offscreen work at 1280x720, 120 warmup + 300 measured frames, with glFinish
included and synthetic canonical audio. It is not APK display pacing or thermal
endurance evidence.

| Template | p50 ms | p95 ms | Stable texture bytes |
| --- | ---: | ---: | ---: |
| Layered neon | 11.63 | 18.51 | 84,227,588 |
| Aurora clouds | 27.45 | 30.96 | 34,444,804 |
| Firefly garden | 17.47 | 22.40 | 85,437,188 |
| Prismatic lotus | 40.34 | 42.95 | 105,769,988 |
| Stellar currents | 23.73 | 28.69 | 98,339,588 |
| Orbital reliquary | 12.58 | 17.74 | 49,177,988 |

All six retained constant renderer texture allocation during the measurement.
Lotus does not satisfy a 30 fps budget at this extent; a mobile render quality
cap is being validated rather than advertising the Windows canvas unconditionally.
Evidence: `out/android-template-measurements/1543cf889e7e4a128035105ab8e32fe1`.
Runner: `tools/measure-android-templates.py`; separate build: `tools/build-android.py`.

The four changed/new Windows packages also pass visible Android native playback;
logs: `out/android-{aurora,firefly,stellar,orbital}-gpu.log`. The new displacement,
RGBA16F trail and device recreation checks are detailed in
`displacement_trails_2026-09-07.md`.

The shared balanced policy caps both long edge and pixel count, preserving
portrait/square framing and never upscaling. At 960x540 the measured p95 values
are 10.25, 15.82, 17.29, 29.62, 21.95 and 11.29 ms respectively, with stable
textures. Evidence: `out/android-template-measurements/bd92c206c45c42b6960a9179973f41bf`.
Android now defaults to this policy and persists original/balanced/economy choice.
These short native measurements do not establish display pacing or battery use.

The Python APK assembler now accepts the CMake build directory/configuration;
otherwise the former hardcoded Debug directory would package the wrong native
binary after a Release build. The corrected artifact is
`out/android-arm64-release/apk/rhythm-player-release.apk`, signed with the same
local acceptance key. The manifest is still a local testing manifest, not a
store release declaration. `out/android-release-apk-corrected.log` verifies it.

Remaining: six-template parameter-extreme/idle motion review, latest Windows
performance/deployment checks, real APK import,
audio/touch/lifecycle acceptance, then further content batches. A connected device
and native GPU success do not resolve the earlier APK installation restriction.
