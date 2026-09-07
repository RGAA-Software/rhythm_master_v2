# Windows deployment acceptance — 2026-09-07

The local Debug acceptance executable is now
`out/windows/src/windows_spike/deploy/rhythm_master.exe`.
Copy the entire `deploy` directory when moving this build.

`tools/deploy-windows.py` recursively inspects PE imports with the configured
MSVC dumpbin, resolves the selected SDK and compiler runtimes, and copies the
13 required DLLs beside both the original executable and the deployed one.
The bundle also contains content, both locales, third-party notices and a
SHA-256 file manifest. Windows system DLLs and installed system fonts remain
OS dependencies. No Python installation is needed to run the packaged app.

CMake generates target metadata and invokes Python after executable linking
and on every default Windows build, including builds without C++ changes.
The launcher now opens the deployed executable without adding SDK paths.
Missing dependencies fail deployment rather than silently leaving a partial
bundle reported as successful.

Verified on the current Windows machine:

- All 11 Windows CTest entries pass, including `windows_deploy_smoke`.
- The deployed app runs from an unrelated working directory with PATH restricted
  to Windows and System32. It renders 30 GPU frames, with 8/8 visible nodes and
  three viewers.
- Runtime module sampling confirms all 13 packaged DLLs load from `deploy`;
  no Qt DLL is observed.
- Removing the generated `deploy/brotlicommon.dll` and rebuilding restores it.
  That build runs resource/deployment steps without recompiling or relinking
  C++, preserves the incremental cache and passes all 11 tests.

Local evidence: `out/windows-build.log`,
`out/windows/deployment-test/stdout.log`,
`out/windows/deployment-test/loaded-modules.txt`, and
`out/windows/src/windows_spike/deploy/deployment-manifest.json`.
This verifies local Debug packaging; clean-machine Release distribution and
other platform packages remain separate acceptance work.
