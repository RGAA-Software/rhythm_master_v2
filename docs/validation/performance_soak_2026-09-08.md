# Thirty-minute music performance soak

The opt-in `performance_soak` executable runs an actual published music work
through `FilePlayback`, the shared music clock, Player and Windows D3D11 at
1280×720. SDL device volume is zero **after** analysis; actual decoded PCM and
device consumption still drive the graph. It uses one audio output stream and
one Player session across repeats. It is intentionally outside default CTest.

Checks cover source acknowledgment, continuous submitted/consumed counters,
bounded device queue and analysis backlog, observed audio, valid render output,
resource budgets and settled texture allocation after warm-up/loop transitions.
After stopping, releasing graphics and draining six frames, graph textures must
be zero. Progress is flushed once per minute. Timing uses a fixed histogram of
0.1 ms buckets, with an overflow bucket; it does not allocate per-frame samples.

## Result

`out/performance-soak-30min.log` completes successfully:

- 1,800 seconds, **107,340 frames**, **112 music loops**.
- Stable and peak graph texture allocation **113,142,788 bytes** (about 108 MiB).
- Measured host-frame p50/p95 **16.7 / 17.2 ms**. Warm-up and the first 120 frames
  of each loop are excluded from the timing/stability sample.
- Three frames enter the final approximately-200-ms histogram bucket. Its
  `frame_ge_200ms` log name is rounded: with ceiling quantization, it includes
  values above 199.9 ms. This is not hitch-free acceptance.
- After teardown, **zero graph texture bytes** remain.

The tested binary uses the continuous-loop implementation at `f294b14` and the
208-instruction, 16-second, four-section arrangement package. Exact binary and
package hashes are in `out/performance-soak-30min.json`. The later Harmonic City
work is validated separately; this run is not its endurance test.

This was a contention/stability run: project builds, D3D tests, GPU captures and
unrelated user builds ran concurrently. They were not terminated. Host wall time
does not isolate GPU execution, and the timing is not an idle-machine baseline.
The checks do not establish hardware audio gaplessness, process/driver memory
leak freedom, thermal behavior or Android application lifecycle. Source PCM may
still have an audible discontinuity at its own loop boundary.

## Reproduce

Build the host and its regular Python-managed DLL deployment first:

```powershell
python tools/build-windows.py --target rhythm_master --target performance_soak
out/windows-release/src/windows_spike/performance_soak.exe "path/to/music.rhythmpack" 1800
```

The duration accepts 60–3,600 seconds. Use a package containing music. For a
controlled frame-time baseline, wait for builds and other GPU tests to finish.
Android APK installation remains blocked by device policy, so this Windows
result does not close the Android application gate.
