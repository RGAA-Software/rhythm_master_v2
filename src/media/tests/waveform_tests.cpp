#include <algorithm>
#include <chrono>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

#include "rhythm/assets/store.h"
#include "rhythm/foundation/blocking_executor.h"
#include "rhythm/media/audio_decoder.h"
#include "rhythm/media/waveform.h"
#include "rhythm/media/waveform_cache.h"
#include "rhythm/media/waveform_index.h"

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
void Intervals() {
    using namespace rhythm;
    media::WaveformOverview overview;
    overview.frames_ = 48000 * 4;
    overview.frames_per_peak_ = 48000;
    overview.count_ = 4;
    overview.peaks_[0] = {-0.1F, 0.2F};
    overview.peaks_[1] = {-0.3F, 0.4F};
    overview.peaks_[2] = {-0.7F, 0.8F};
    overview.peaks_[3] = {-0.5F, 0.6F};
    const media::WaveformIndex waveform(overview);
    Check(waveform.Range(1, 3).minimum_ == -0.7F && waveform.Range(1, 3).maximum_ == 0.8F,
          "range query preserves both-polarity peaks across bins");
    Check(waveform.Range(4, 5).maximum_ == 0 && waveform.Range(0, 0).maximum_ == 0,
          "empty and out-of-source ranges are silent");
    parameters::ClipTiming timing{10, 8, 1, 3};
    const parameters::ClipInterval blank(timing);
    Check(media::ClipWaveformPeak(waveform, blank, 10, 11).maximum_ == 0.4F &&
                  media::ClipWaveformPeak(waveform, blank, 11, 12).maximum_ == 0.8F &&
                  media::ClipWaveformPeak(waveform, blank, 12, 13).maximum_ == 0,
          "placed source crop has native speed and a blank tail");
    timing.end_ = parameters::ClipEnd::kLoop;
    const parameters::ClipInterval loop(timing);
    Check(media::ClipWaveformPeak(waveform, loop, 12, 13).maximum_ == 0.4F &&
                  media::ClipWaveformPeak(waveform, loop, 11.5, 12.5).maximum_ == 0.8F &&
                  media::ClipWaveformPeak(waveform, loop, 10, 18).maximum_ == 0.8F,
          "loop boundaries and multiple repetitions preserve the cropped source envelope");
}
}  // namespace
int main(int argc, char* argv[]) {
    using namespace rhythm::media;
    try {
        Check(argc == 2, "waveform_tests music");
        Peaks();
        Intervals();
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
        const auto directory = std::filesystem::path(argv[1]).parent_path() / "waveform-assets";
        rhythm::assets::Store store(directory);
        auto asset = store.Import(argv[1], "audio/wav");
        Check(scanner.StartAsset(directory, asset), "asset scan admitted on the same worker");
        const auto asset_result = drain();
        Check(asset_result.overview_ && asset_result.overview_->frames_ == overview.frames_ &&
                      asset_result.overview_->count_ == overview.count_,
              "verified immutable asset uses the same complete waveform decoder");
        ++asset.bytes_;
        Check(scanner.StartAsset(directory, asset), "invalid metadata is checked asynchronously");
        Check(!drain().overview_, "invalid asset cannot publish a waveform");
        --asset.bytes_;
        WaveformCache cache;
        const std::array records{asset, asset};
        const auto cache_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (cache.ReadyCount() != 1 && std::chrono::steady_clock::now() < cache_deadline) {
            cache.Update(directory, records);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        const auto shared = cache.Find(asset.id_);
        Check(shared && shared->Duration() == 128 && !cache.Busy(),
              "repeated clips share one completed source without retaining a worker");
        cache.Update(directory, records);
        Check(cache.Find(asset.id_) == shared && !cache.Busy(), "warm cache does not decode again");
        // The standalone scanner owns one of eight pool slots. A warm waveform
        // cache must leave all other seven available to editor/export services.
        std::vector<std::unique_ptr<rhythm::foundation::BlockingExecutor>> workers;
        for (int index = 0; index < 7; ++index)
            workers.push_back(std::make_unique<rhythm::foundation::BlockingExecutor>(
                    rhythm::foundation::BlockingOptions{1, 1}));
        workers.clear();
        cache.Clear();
        Check(!cache.Find(asset.id_) && !cache.Busy() && shared->Duration() == 128,
              "cache eviction leaves borrowed immutable frame ownership valid");
        cache.Update(directory, records);
        cache.Clear();
        const auto cancel_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (cache.Busy() && std::chrono::steady_clock::now() < cancel_deadline) {
            cache.Clear();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        Check(!cache.Busy() && !cache.Find(asset.id_),
              "canceled late result cannot refill the cache");
        cache.Update(directory, records);
        cache.Clear();
        const auto reopen_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (cache.ReadyCount() != 1 && std::chrono::steady_clock::now() < reopen_deadline) {
            cache.Update(directory, records);
            Check(cache.FailureCount() == 0,
                  "canceled result cannot fail a newly reopened request for the same source");
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        Check(cache.ReadyCount() == 1 && !cache.Busy(), "immediate reopen eventually rescans");
        std::cout << "bounded waveform peaks, exact file envelope, cancellation and errors pass\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
