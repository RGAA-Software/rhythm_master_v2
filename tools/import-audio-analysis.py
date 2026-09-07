"""Reproducible, focused first-party audio extraction with separate FFT attribution."""

import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SOURCE = Path("C:/source/shark_dynamics_wallpaper")
REFERENCE = Path("C:/source/reference/projectm")
REVISION = "118dbc811718836ffb4ca62eec7384605df5be32"
REFERENCE_REVISION = "359bf7801e20b87712d6a89c8862f770c84ecdeb"


def main():
    for path, revision in ((SOURCE, REVISION), (REFERENCE, REFERENCE_REVISION)):
        if subprocess.check_output(["git", "-C", str(path), "rev-parse", "HEAD"], text=True).strip() != revision:
            raise SystemExit("Audio source revision mismatch")
    destination = ROOT / "src/audio_analysis/detail"
    if destination.exists():
        raise SystemExit("Extraction already exists; maintain focused edits instead of overwriting")
    destination.mkdir(parents=True)
    notice_source = REFERENCE / "src/libprojectM/Audio/MilkdropFFT.cpp"
    notice = notice_source.read_text(encoding="utf-8").split("*/", 1)[0] + "*/\n"
    notices = ROOT / "third_party/notices/audio_fft"
    notices.mkdir(parents=True, exist_ok=True)
    (notices / "LICENSE.txt").write_text(notice, encoding="utf-8")
    names = {"sk_band_map": "band_map", "sk_feat_fft": "fft", "sk_onset": "onset", "sk_bpm": "tempo"}
    tokens = {"SkBandMap": "BandMap", "SkFeatFft": "Fft", "SkOnsetEvent": "OnsetEvent",
              "SkOnset": "Onset", "SkBpm": "Tempo", "SkConfig": "Config", "PI": "kPi",
              "fftSize": "fft_size", "sampleRate": "sample_rate", "numBands": "num_bands",
              "minFreq": "min_freq", "maxFreq": "max_freq", "bandsOut": "bands_out",
              "numBins": "num_bins", "freqLow": "freq_low", "freqHigh": "freq_high",
              "inverseNumBins": "inverse_num_bins", "dftSize": "dft_size", "halfSize": "half_size",
              "magOut": "mag_out", "applyWindow": "apply_window", "applyEqualize": "apply_equalize",
              "spectrumData": "spectrum_data", "thresholdRatio": "threshold_ratio_",
              "refractorySec": "refractory_seconds_", "meanWindow": "mean_window_", "warmupSec": "warmup_seconds_",
              "frameTime": "frame_time", "isLocalPeak": "is_local_peak", "aboveThreshold": "above_threshold",
              "refractoryOk": "refractory_ok", "refractoryFrames": "refractory_frames",
              "minBpm": "min_bpm_", "maxBpm": "max_bpm_", "maxOnsets": "max_onsets_",
              "minOnsets": "min_onsets_", "minConfidence": "min_confidence_",
              "binCount": "bin_count", "binIois": "bin_iois", "bestBin": "best_bin", "meanIoi": "mean_ioi"}
    files = []
    for source_name, target_name in names.items():
        for suffix in (".h", ".cpp"):
            source = SOURCE / "src/audio/feature" / (source_name + suffix)
            original = source.read_bytes()
            code = original.decode("utf-8-sig")
            for before, after in names.items():
                code = code.replace(before + ".h", after + ".h")
            for before, after in tokens.items():
                code = re.sub(r"\b" + before + r"\b", after, code)
            code = code.replace("namespace sk {\nnamespace audio {", "namespace rhythm::audio::detail {")
            code = re.sub(r"}\s*// namespace audio\s*}\s*// namespace sk", "}  // namespace rhythm::audio::detail", code)
            code = code.replace("#include <cstddef>", "#include <cstddef>\n#include <span>")
            code = code.replace("const float* magnitudes", "std::span<const float> magnitudes")
            code = code.replace("float* bands_out", "std::span<float> bands_out")
            code = code.replace("std::complex<float>* data", "std::span<std::complex<float>> data")
            code = code.replace("const float* samples", "std::span<const float> samples")
            code = code.replace("Transform(spectrum_data.data())", "Transform(spectrum_data)")
            code = code.replace("reordered.end(), data)", "reordered.end(), data.begin())")
            code = code.replace("prev_magnitudes_.assign(magnitudes, magnitudes + num_bins)",
                                "prev_magnitudes_.assign(magnitudes.begin(), magnitudes.end())")
            if target_name == "onset":
                code = code.replace("double timeSec", "double time_seconds").replace("timeSec", "time_seconds_")
                code = code.replace("double time_seconds =", "double time_seconds_ =")
                code = code.replace("std::size_t frame = 0;", "std::size_t frame_ = 0;")
                code = code.replace("float strength =", "float strength_ =")
                code = code.replace("last_onset_.frame", "last_onset_.frame_")
                code = code.replace("last_onset_.strength", "last_onset_.strength_")
            else:
                code = code.replace("timeSec", "time_seconds")
            if target_name == "fft":
                code = notice + code
            target = destination / (target_name + suffix)
            target.write_text(code, encoding="utf-8")
            files.append({"source": source.relative_to(SOURCE).as_posix(),
                          "source_sha256": hashlib.sha256(original).hexdigest(),
                          "target": target.relative_to(ROOT).as_posix()})
    record = {"source_url": "https://github.com/RGAA-Software/rhythm_master", "revision": REVISION,
              "files": files, "ownership": "First-party extraction; no project outbound license selected",
              "modifications": ["Google naming and four-space format", "Private implementation APIs use spans",
                                "Bounded input validation and reuse of FFT scratch storage added after extraction",
                                "Onset local peak indexing corrected against synthetic impulses"],
              "embedded_reference": {"source_url": "https://github.com/projectM-visualizer/projectm",
                                     "revision": REFERENCE_REVISION,
                                     "files": ["src/libprojectM/Audio/MilkdropFFT.cpp", "src/libprojectM/Audio/MilkdropFFT.hpp"],
                                     "license": "BSD-3-Clause, Copyright 2005-2013 Nullsoft, Inc.",
                                     "notice": "third_party/notices/audio_fft/LICENSE.txt"},
              "excluded": ["projectM-derived envelope and waveform alignment pending separate license integration review",
                           "VLC/Qt hosts, legacy smoothing and non-audio effects"]}
    (ROOT / "provenance/audio_analysis.json").write_text(json.dumps(record, indent=4) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
