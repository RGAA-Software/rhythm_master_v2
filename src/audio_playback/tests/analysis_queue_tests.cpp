#include <cmath>
#include <iostream>
#include <stdexcept>

#include "analysis_queue.h"

namespace {
void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
rhythm::media::AudioBlock Block(std::uint64_t generation, std::uint64_t first) {
    rhythm::media::AudioBlock block;
    block.generation_ = generation;
    block.first_sample_ = first;
    block.samples_.resize(8192);
    for (std::size_t index = 0; index < 4096; ++index) {
        const auto sample = static_cast<float>(0.5 * std::sin((first + index) * 0.1));
        block.samples_[index * 2] = block.samples_[index * 2 + 1] = sample;
    }
    return block;
}
}  // namespace
int main() {
    using namespace rhythm;
    try {
        audio::detail::AnalysisQueue queue(1, 1200);
        queue.Append(Block(1, 1200));
        queue.Append(Block(1, 5296));
        queue.Append(Block(2, 0));
        queue.Append(Block(2, 4096));
        Check(queue.Generation() == 1 && queue.Position() == 1200 && !queue.Snapshot(),
              "decode-ahead loop does not change audible generation or position");
        queue.Consume(8191);
        Check(queue.Generation() == 1 && queue.Position() == 9391 && queue.Snapshot() &&
                      queue.Snapshot()->generation_ == 1,
              "old tail analysis retained before audible boundary");
        queue.Consume(8192);
        Check(queue.Generation() == 1 && queue.Position() == 9392,
              "boundary does not consume the first sample twice");
        queue.Consume(8193);
        Check(queue.Generation() == 2 && queue.Position() == 1 && !queue.Snapshot(),
              "first loop sample resets analysis and local time at consumption");
        queue.Consume(14192);
        Check(queue.Snapshot() && queue.Snapshot()->generation_ == 2 &&
                      queue.Snapshot()->center_seconds_ < 6000.0 / 48000 &&
                      queue.Position() == 6000,
              "loop feature timestamps stay in local sample coordinates");
        queue.Consume(16384);
        Check(queue.Position() == 8192 && queue.Consumed() == 16384,
              "device cursor remains continuous across local-time reset");
        const auto reject = [&](media::AudioBlock block) {
            bool rejected = false;
            try {
                queue.Append(std::move(block));
            } catch (const std::invalid_argument&) {
                rejected = true;
            }
            Check(rejected, "invalid loop sequence accepted");
        };
        reject(Block(1, 0));
        reject(Block(3, 1));
        reject(Block(2, 9000));
        queue.Append(Block(3, 0));
        queue.Consume(20480);
        Check(queue.Generation() == 3 && queue.Position() == 4096,
              "rejected append preserves next valid loop");
        for (std::uint64_t index = 1; index <= 8; ++index) queue.Append(Block(3, index * 4096));
        reject(Block(3, 9 * 4096));
        queue.Consume(53248);
        Check(queue.Position() == 9 * 4096 && queue.Snapshot(), "bounded backlog drains exactly");
        for (std::uint64_t generation = 4; generation < 12; ++generation) {
            auto small = Block(generation, 0);
            small.samples_.resize(2);
            queue.Append(std::move(small));
        }
        auto excessive_capacity = Block(12, 0);
        excessive_capacity.samples_.resize(2);
        reject(std::move(excessive_capacity));
        queue.Consume(53256);
        Check(queue.Generation() == 11 && queue.Position() == 1,
              "tiny loop blocks account for allocated capacity and release it on consume");
        std::cout << "loop consumption boundaries, feature generations and PCM queue bounds pass\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
