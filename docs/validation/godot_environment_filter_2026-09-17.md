# Godot environment filter validation — 2026-09-17

W1.3 replaces the fixed 64-sample environment prefilter behavior with the
focused Godot 4.5.1 GLES algorithm: four filtered specular levels use
8/16/32/128 samples, perceptual roughness is transformed before GGX sampling,
and accepted radiance is normalized by `N·L`. Scene sampling now uses Godot's
`sqrt(roughness)` LOD, roughness-squared reflection bending and horizon
attenuation. The existing linear 780×66 RGBA16F atlas, project handles and one
preparation pass remain unchanged.

The permanent GPU probe constructs a 128×64 linear Float16 environment with a
narrow HDR highlight. A white metal receiver at roughness
0/.0625/.25/.5625/1 reads back 192/185/110/22/9 on D3D11. The strict descending
ladder and bounded numerical baselines are asserted together with constant
environment energy, sRGB decoding, alpha unpremultiplication, diffuse, AO,
rotation and HDR values above one.

Windows and Android GLES 3.0 shader compilation pass. The final Windows probe
passed at
`out/w13-godot-environment-submit.log.runs/1789578599314810800.log`.
Android device output is intentionally deferred to the final platform stage.

The current compiled Sonic Enamel author graph was then exercised for 121
frames at 640×360, including its autonomous camera sway, rotating environment,
Hexagon medium DOF and point shadow. It passed with stable texture bytes
74,301,892 (point shadow) / 23,970,244 (no shadow), and a maximum point-shadow
RGB difference of 0.320761. Log:
`out/w13-sonic-enamel-work.log.runs/1789578389707500500.log`.

The remaining compatibility gap is explicit: the public environment source is
a single equirectangular 2D texture without Godot's source-cubemap mip chain, so
the source mip/PDF term is not imported. The fixed atlas resolution continues
to bound mirror detail. No dependency or public renderer type was added.
