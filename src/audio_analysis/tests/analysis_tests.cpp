#include <algorithm>
#include <complex>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <vector>

#include "detail/fft.h"
#include "detail/onset.h"
#include "detail/tempo.h"
#include "rhythm/audio/analyzer.h"

namespace {
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
std::vector<float> Tone(std::uint32_t rate, std::size_t frames, double frequency,
                        double amplitude) {
    std::vector<float> result(frames * 2);
    for (std::size_t index = 0; index < frames; ++index) {
        result[index * 2] =
                static_cast<float>(amplitude * std::sin(2 * std::numbers::pi * frequency *
                                                        static_cast<double>(index) / rate));
        result[index * 2 + 1] = 0;
    }
    return result;
}
rhythm::audio::Features Analyze(std::uint32_t rate, std::span<const float> samples,
                                std::size_t chunk) {
    rhythm::audio::Analyzer analyzer;
    Require(analyzer.Reset(rate, 1), "reset");
    std::size_t position = 0;
    while (position < samples.size() / 2) {
        const auto count = std::min(chunk, samples.size() / 2 - position);
        Require(analyzer.Push(samples.subspan(position * 2, count * 2), 2, position), "push");
        position += count;
    }
    return analyzer.Snapshot();
}
void FftOracle() {
    rhythm::audio::detail::Fft fft(32);
    std::vector<std::complex<float>> values(32);
    for (std::size_t index = 0; index < values.size(); ++index)
        values[index] = {static_cast<float>(index % 7) / 8, static_cast<float>(index % 3) / 9};
    const auto input = values;
    fft.Transform(values);
    for (std::size_t bin = 0; bin < values.size(); ++bin) {
        std::complex<double> expected{};
        for (std::size_t index = 0; index < input.size(); ++index)
            expected += std::complex<double>(input[index]) *
                        std::polar(1.0, -2 * std::numbers::pi * static_cast<double>(bin * index) /
                                                static_cast<double>(input.size()));
        Require(std::abs(std::complex<double>(values[bin]) - expected) < 0.00002,
                "fft.independent_dft");
    }
}
void Spectrum() {
    for (const auto rate : {44100U, 48000U}) {
        for (const auto frequency : {110.0, 1000.0, 8000.0}) {
            for (const auto amplitude : {0.01, 0.5}) {
                const auto samples = Tone(rate, rate, frequency, amplitude);
                const auto frame = Analyze(rate, samples, 1024);
                Require(frame.valid_ && rhythm::audio::ValidFeatures(frame), "features.valid");
                Require(std::abs(frame.rms_ - amplitude / 2) < amplitude * 0.02, "stereo.rms");
                Require(std::abs(frame.spectral_centroid_hz_ - frequency) < 25, "centroid.hz");
                Require(*std::max_element(frame.right_bands_.begin(), frame.right_bands_.end()) ==
                                0,
                        "stereo.isolation");
                const auto peak =
                        std::max_element(frame.left_bands_.begin(), frame.left_bands_.end());
                const auto band = static_cast<double>(peak - frame.left_bands_.begin());
                const auto expected = std::log(frequency / 20) / std::log(800.0) * 63;
                Require(std::abs(band - expected) < 1.5 && *peak > 0.005, "spectrum.tone_band");
                const auto fragmented = Analyze(rate, samples, 127);
                Require(frame.mono_bands_ == fragmented.mono_bands_ &&
                                frame.end_sample_ == fragmented.end_sample_ &&
                                frame.bpm_ == fragmented.bpm_,
                        "chunking.invariance");
            }
        }
    }
}
void Boundaries() {
    rhythm::audio::Analyzer analyzer;
    Require(!analyzer.Reset(0, 1) && !analyzer.Reset(48000, 0), "reset.invalid");
    Require(analyzer.Reset(48000, 1), "reset.initial");
    std::vector<float> samples(800, 0);
    Require(!analyzer.Push(samples, 3, 0) && !analyzer.Push(samples, 1, 1), "input.shape_position");
    samples.back() = std::numeric_limits<float>::quiet_NaN();
    Require(!analyzer.Push(samples, 1, 0), "input.nan_atomic");
    samples.back() = 0;
    Require(analyzer.Push(samples, 1, 0) && !analyzer.Snapshot().valid_, "warmup");
    Require(!analyzer.Reset(48000, 1), "generation.replay");
    Require(analyzer.Reset(48000, 2, 96000), "seek.reset");
    for (std::uint64_t index = 0; index < 10; ++index)
        Require(analyzer.Push(samples, 1, 96000 + index * 800), "silence.push");
    const auto frame = analyzer.Snapshot();
    Require(frame.valid_ && frame.rms_ == 0 && frame.bpm_ == 0 && frame.onset_id_ == 0,
            "silence.features");
    Require(frame.end_sample_ == 104000 && frame.center_seconds_ > 2, "source.timestamps");
    auto invalid = frame;
    invalid.mono_bands_[0] = std::numeric_limits<float>::infinity();
    Require(!rhythm::audio::ValidFeatures(invalid), "snapshot.reject_invalid");
}
void Rhythm() {
    rhythm::audio::detail::Onset onset;
    std::array<float, 63> bands{};
    std::size_t detections = 0;
    for (std::size_t frame = 0; frame < 240; ++frame) {
        bands.fill(frame % 30 == 0 ? 1.0f : 0.0f);
        onset.Process(bands, bands.size(), frame, static_cast<double>(frame) / 60, 1.0 / 60);
        if (onset.HasOnset()) {
            Require(onset.LastOnset().frame_ % 30 == 0 && frame == onset.LastOnset().frame_ + 1,
                    "onset.peak_timestamp");
            ++detections;
        }
    }
    Require(detections >= 6, "onset.impulses");
    rhythm::audio::detail::Tempo tempo;
    for (int index = 0; index < 20; ++index) tempo.AddOnset(index * 0.5);
    Require(std::abs(tempo.Bpm() - 120) < 0.001 && tempo.Confidence() == 1, "tempo.120");
    tempo.Reset();
    Require(tempo.Bpm() == 0 && tempo.Confidence() == 0, "tempo.reset");
}
}  // namespace
int main() {
    try {
        FftOracle();
        Spectrum();
        Boundaries();
        Rhythm();
        std::cout << "Audio analysis passed: DFT oracle, stereo tones, quiet signals, chunk "
                     "invariance, PCM clocks, onset and tempo\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
