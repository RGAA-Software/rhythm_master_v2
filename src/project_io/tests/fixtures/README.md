# Legacy runtime fixture

`legacy-v1.rhythmpack` is generated project-owned test content from the original
`signal_texture` template and the project's ABI 1 publisher, retained before the
ABI 2 canvas change on 2026-09-07. It contains no imported assets.
SHA-256: `6a79d3c755b10398cdb54f92b88599f176e3d355c6a3656cbe9023124c77c3ea`.

The compatibility test must read these frozen old bytes, not regenerate a package
with the current publisher. Its expected canvas is 640x360.
