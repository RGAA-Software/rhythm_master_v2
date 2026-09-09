#include <cmath>
#include <iostream>
#include <stdexcept>

#include "playback_presentation.h"

namespace {
using namespace rhythm;
using namespace rhythm::audio::detail;
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
StreamPcm Block(StreamIdentity identity, std::uint64_t first,
                std::optional<StreamPosition> incoming = {}) {
    StreamPcm block{{}, identity, first, incoming};
    block.samples_.resize(8192);
    for (std::size_t index = 0; index < 4096; ++index) {
        const auto sample = static_cast<float>(0.2 * std::sin((first + index) * 0.05) +
                                               0.3 * std::sin((first + index) * 0.17));
        block.samples_[index * 2] = block.samples_[index * 2 + 1] = sample;
    }
    return block;
}
void Run() {
    PlaybackPresentation presentation(1, {{10, 0}, 1200});
    const auto first = Block({10, 0}, 1200, StreamPosition{{20, 0}, 9000});
    const auto second = Block({10, 0}, 5296, StreamPosition{{20, 1}, 0});
    presentation.Append(first, 1);
    presentation.Append(second, 1);
    presentation.Append(Block({20, 1}, 4096), 2);
    Require(presentation.Position() == 1200 && presentation.Sources().current_.sample_ == 1200 &&
                    !presentation.Sources().incoming_ && !presentation.FeaturesSnapshot(),
            "decode ahead does not present either source or features");
    presentation.Consume(4096);
    auto sources = presentation.Sources();
    Require(sources.current_.sample_ == 5296 && sources.incoming_ &&
                    sources.incoming_->identity_ == StreamIdentity{20, 0} &&
                    sources.incoming_->sample_ == 13096,
            "last frame before incoming loop retains its exact prior iteration");
    presentation.Consume(4097);
    sources = presentation.Sources();
    Require(presentation.Generation() == 1 && sources.current_.sample_ == 5297 &&
                    sources.incoming_->identity_ == StreamIdentity{20, 1} &&
                    sources.incoming_->sample_ == 1,
            "incoming loop changes independently of old scene and mixed FFT generation");
    presentation.Consume(8192);
    audio::Analyzer reference;
    Require(reference.Reset(media::kAudioSampleRate, 1, 1200) &&
                    reference.Push(first.samples_, 2, 1200) &&
                    reference.Push(second.samples_, 2, 5296),
            "reference mixed PCM analysis");
    Require(presentation.FeaturesSnapshot() == std::optional(reference.Snapshot()),
            "one consumed mixed FFT exactly matches the canonical analyzer");
    const auto paused_sources = presentation.Sources();
    presentation.Consume(8192);
    Require(presentation.Sources().current_.sample_ == paused_sources.current_.sample_ &&
                    presentation.Sources().incoming_->sample_ == paused_sources.incoming_->sample_,
            "unchanged consumed count freezes both scene clocks");
    presentation.Consume(8193);
    sources = presentation.Sources();
    Require(presentation.Generation() == 2 && presentation.Position() == 4097 &&
                    sources.current_.identity_ == StreamIdentity{20, 1} &&
                    sources.current_.sample_ == 4097 && !sources.incoming_ &&
                    !presentation.FeaturesSnapshot(),
            "audible handoff publishes actual incoming sample and clears old FFT");
    presentation.Consume(12288);

    bool invalid = false;
    try {
        presentation.Append(Block({20, 0}, 0), 3);
    } catch (const std::invalid_argument&) {
        invalid = true;
    }
    Require(invalid, "loop identity cannot move backward");
    invalid = false;
    try {
        presentation.Append(Block({20, 1}, 9999), 2);
    } catch (const std::invalid_argument&) {
        invalid = true;
    }
    Require(invalid, "analysis rejection rolls back matching metadata");
    presentation.Append(Block({10, 0}, 13488), 3);  // Old source ID is intentional rollback.
    presentation.Consume(12289);
    Require(presentation.Generation() == 3 &&
                    presentation.Sources().current_.identity_.source_ == 10 &&
                    presentation.Position() == 13489 &&
                    presentation.Sources().current_.sample_ == 13489,
            "rollback uses a fresh analysis generation with the stable old source identity");
    presentation.Consume(16384);
    Require(presentation.Consumed() == 16384 && presentation.Position() == 17584,
            "all metadata and PCM drain to the same sample");
}
}  // namespace
int main() {
    try {
        Run();
        std::cout << "dual source consumption: exact loop clocks, mixed FFT, pause, handoff and "
                     "rollback passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
