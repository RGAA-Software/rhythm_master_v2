#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

#include "rhythm/media/audio_decoder.h"
#include "rhythm/media/waveform.h"

namespace {
void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
void Peaks() {
    using namespace rhythm::media;
    WaveformAccumulator accumulator;
    std::array<float, 8192> samples{};
    // Sparse opposite-polarity impulses around every compaction boundary must
    // survive; averaging stereo or dropping old bins would lose this structure.
    for (std::uint64_t block = 0; block < 2101; ++block) {
        samples.fill(0);
        if (block % 255 == 0) {
            samples.front() = 0.75F;
            samples[1] = -0.875F;
        }
        accumulator.Append(samples, block * 4096);
    }
    const auto& overview = accumulator.Overview();
    Check(overview.frames_ == 2101 * 4096ULL && overview.count_ <= kMaximumWaveformBins &&
                  overview.frames_per_peak_ > 256,
          "bounded adaptive overview with exact sample extent");
    for (std::uint64_t block = 0; block < 2101; block += 255) {
        const auto& peak = overview.peaks_[block * 4096 / overview.frames_per_peak_];
        Check(peak.maximum_ == 0.75F && peak.minimum_ == -0.875F,
              "opposite channel peaks preserved at their exact interval");
    }
    bool rejected = false;
    try {
        accumulator.Append(samples, 0);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    Check(rejected, "discontinuous PCM rejected");
}
}  // namespace
int main(int argc, char* argv[]) {
    using namespace rhythm::media;
    try {
        Check(argc == 2, "waveform_tests music");
        Peaks();
        WaveformScanner scanner;
        const auto drain = [&] {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
            while (std::chrono::steady_clock::now() < deadline) {
                if (auto result = scanner.Take()) return *result;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            throw std::runtime_error("waveform test timeout");
        };
        Check(scanner.Start(argv[1]) && !scanner.Start(argv[1]), "one admitted scan");
        auto result = drain();
        Check(result.overview_.has_value(), "file scan succeeds");
        const auto& overview = *result.overview_;
        Check(overview.Duration() == 128 && overview.count_ <= kMaximumWaveformBins,
              "complete 128-second bounded waveform");
        // Independent full decode checks every sample against its recorded bin.
        AudioDecoder decoder(argv[1]);
        float maximum = 0;
        while (auto block = decoder.Read()) {
            for (std::size_t offset = 0; offset < block->samples_.size(); ++offset) {
                const auto frame = block->first_sample_ + offset / 2;
                const auto& peak = overview.peaks_[frame / overview.frames_per_peak_];
                const auto sample = block->samples_[offset];
                Check(sample >= peak.minimum_ && sample <= peak.maximum_,
                      "envelope contains decoded sample");
                maximum = std::max(maximum, std::abs(sample));
            }
        }
        Check(maximum > 0.1F && scanner.SecondsScanned() == 128, "real audio and exact progress");
        Check(scanner.Start(argv[1]), "scanner reusable");
        scanner.Cancel();
        Check(!drain().overview_, "canceled scan never publishes an overview");
        Check(scanner.Start(std::filesystem::path(argv[1]).parent_path() / "missing.music"),
              "invalid source admitted asynchronously");
        Check(!drain().overview_, "invalid source fails without replacing a result");
        std::cout << "bounded waveform peaks, exact file envelope, cancellation and errors pass\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
