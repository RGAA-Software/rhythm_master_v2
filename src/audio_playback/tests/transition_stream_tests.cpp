#include <cmath>
#include <iostream>
#include <stdexcept>

#include "transition_stream.h"

namespace {
using namespace rhythm;
using namespace rhythm::audio::detail;
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
std::vector<float> Decode(const PlaybackSource& source) {
    AudioStream stream(source, 1, {});
    std::vector<float> samples;
    while (auto block = stream.Read({}))
        samples.insert(samples.end(), block->samples_.begin(), block->samples_.end());
    return samples;
}
PlaybackSource Arrangement(const storage::FileBytes& file, std::size_t count, double delay = 0) {
    const assets::AssetId id{std::string(64, 'a')};
    std::vector<media::AudioClip> clips;
    for (std::size_t index = 0; index < count; ++index) {
        media::AudioClip clip{index + 1, "Clip", id};
        clip.timing_ = {delay, 0.2, 0.2, 0.4};
        clip.gain_ = 0.2F;
        clips.push_back(clip);
    }
    return std::make_shared<const media::AudioArrangementSource>(
            media::AudioArrangementSource{media::AudioArrangement(clips), {{id, {}, file}}});
}
void Run(const std::filesystem::path& directory) {
    const PlaybackSource ramp = directory / std::filesystem::path(u8"音乐 ramp.wav");
    const PlaybackSource tone = directory / "tone44100.wav";
    const auto old_pcm = Decode(ramp);
    const auto next_pcm = Decode(tone);
    TransitionStream stream(ramp, {1, 0.7F, false});
    const auto first = stream.Read();
    Require(first && first->identity_ == StreamIdentity{1, 0}, "initial source identity");
    const auto origin = first->samples_.size() / 2;
    constexpr std::uint64_t kFadeFrames = 9601;  // Not a decoder block multiple.
    stream.Begin(tone, {2, 0.3F, false}, kFadeFrames, media::CrossfadeCurve::kLinear);
    std::uint64_t position = 0;
    while (position < kFadeFrames) {
        const auto block = stream.Read();
        Require(block && block->identity_ == StreamIdentity{1, 0} &&
                        block->first_sample_ == origin + position,
                "old timeline during fade");
        for (std::size_t index = 0; index < block->samples_.size(); ++index) {
            const double fraction = double(position + index / 2) / kFadeFrames;
            const auto old_index = (origin + position) * 2 + index;
            const auto new_index = position * 2 + index;
            const double expected = old_pcm[old_index] * double(0.7F) * (1 - fraction) +
                                    next_pcm[new_index] * double(0.3F) * fraction;
            Require(std::abs(block->samples_[index] - expected) < 1e-6,
                    "mixed PCM matches independent per-sample reference");
        }
        position += block->samples_.size() / 2;
    }
    Require(position == kFadeFrames && stream.Progress().clipped_samples_ == 0,
            "exact nonaligned fade boundary");
    const auto next = stream.Read();
    Require(next && next->identity_ == StreamIdentity{2, 0} && next->first_sample_ == kFadeFrames &&
                    stream.Progress().state_ == FadeState::kSubmitted &&
                    stream.ActiveCursors() == 1,
            "promoted source has advanced through the mixed frames");
    for (std::size_t index = 0; index < next->samples_.size(); ++index)
        Require(std::abs(next->samples_[index] - next_pcm[kFadeFrames * 2 + index] * 0.3F) < 1e-6,
                "incoming PCM remainder preserved across boundary");
    Require(!stream.Cancel(), "completed PCM cannot be retracted without host rollback");

    TransitionStream failure(ramp, {10});
    auto consumed = failure.Read()->samples_.size() / 2;
    bool rejected = false;
    try {
        failure.Begin(directory / "missing.wav", {11}, 24000, media::CrossfadeCurve::kLinear);
    } catch (const std::exception&) {
        rejected = true;
    }
    auto continued = failure.Read();
    Require(rejected && continued->first_sample_ == consumed && failure.ActiveCursors() == 1,
            "failed preparation leaves old cursor untouched and releases lease");
    consumed += continued->samples_.size() / 2;
    failure.Begin(tone, {12}, 24000, media::CrossfadeCurve::kLinear);
    const auto partial = failure.Read();
    consumed += partial->samples_.size() / 2;
    Require(failure.Cancel() && failure.Progress().state_ == FadeState::kCanceled,
            "pending fade canceled");
    continued = failure.Read();
    Require(continued->first_sample_ == consumed && failure.ActiveCursors() == 1,
            "cancel keeps old cursor at next unsubmitted frame");
    for (std::size_t index = 0; index < continued->samples_.size(); ++index)
        Require(continued->samples_[index] == old_pcm[consumed * 2 + index],
                "cancel restores full old gain");
    failure.Begin(tone, {13}, 100, media::CrossfadeCurve::kLinear);
    rejected = false;
    try {
        failure.Begin(ramp, {14}, 100, media::CrossfadeCurve::kLinear);
    } catch (const std::logic_error&) {
        rejected = true;
    }
    Require(rejected && failure.Progress().source_id_ == 13, "busy fade cannot be replaced");
    failure.Seek(1000);
    Require(failure.Progress().state_ == FadeState::kNone &&
                    failure.Read()->first_sample_ == 1000 && failure.ActiveCursors() == 1,
            "seek discards fade and buffered PCM");
    std::stop_source canceled;
    canceled.request_stop();
    rejected = false;
    try {
        failure.Begin(tone, {14}, 100, media::CrossfadeCurve::kLinear, canceled.get_token());
    } catch (const std::exception&) {
        rejected = true;
    }
    Require(rejected && failure.ActiveCursors() == 1, "canceled preparation has no lease");
    failure.Begin(tone, {15}, 0, media::CrossfadeCurve::kLinear);
    Require(failure.Read()->identity_.source_ == 15 && failure.ActiveCursors() == 1,
            "zero-duration handoff emits new source immediately");

    const auto file =
            storage::FileBytes::Open(std::get<std::filesystem::path>(ramp), 16 * 1024 * 1024);
    const auto two = Arrangement(file, 2);
    TransitionStream budget(two, {20});
    budget.Read();
    budget.Begin(two, {21}, 4800, media::CrossfadeCurve::kLinear);
    Require(budget.ActiveCursors() == 4 && budget.PeakCursors() == 4,
            "two scenes share four cursors");
    while (budget.Progress().state_ == FadeState::kMixing) budget.Read();
    Require(budget.ActiveCursors() == 2, "old mixer leases released after promotion");
    const auto late_four = Arrangement(file, 4, 0.5);
    TransitionStream limited(tone, {30});
    const auto before_limit = limited.Read();
    rejected = false;
    try {
        limited.Begin(late_four, {31}, 48000, media::CrossfadeCurve::kLinear);
    } catch (const std::length_error&) {
        rejected = true;
    }
    Require(rejected && limited.Read()->first_sample_ == before_limit->samples_.size() / 2,
            "future four-clip overlap rejected before fade, old PCM continues");

    // A later missing asset is deliberately not touched by the first probe.
    // It must fail the incoming lane, not interrupt the old lane's next sample.
    auto broken = std::get<std::shared_ptr<const media::AudioArrangementSource>>(two)
                          ->arrangement_.Clips();
    broken.resize(1);
    broken[0].timing_ = {0.12, 0.2, 0, 0.2};
    const auto invalid = std::make_shared<const std::vector<std::uint8_t>>(32, std::uint8_t{0});
    const PlaybackSource late_broken =
            std::make_shared<const media::AudioArrangementSource>(media::AudioArrangementSource{
                    media::AudioArrangement(broken), {{broken[0].asset_, invalid, {}}}});
    TransitionStream recovery(ramp, {40});
    recovery.Begin(late_broken, {41}, 24000, media::CrossfadeCurve::kLinear);
    std::uint64_t produced = 0;
    while (recovery.Progress().state_ != FadeState::kFailed) {
        const auto block = recovery.Read();
        Require(block && block->first_sample_ == produced && produced < 24000,
                "late failure keeps old PCM continuous");
        produced += block->samples_.size() / 2;
        if (recovery.Progress().state_ == FadeState::kFailed)
            for (std::size_t index = 0; index < block->samples_.size(); ++index)
                Require(block->samples_[index] == old_pcm[block->first_sample_ * 2 + index],
                        "late source error returns full current source PCM");
    }
    Require(!recovery.Progress().error_.empty() && recovery.ActiveCursors() == 1,
            "late source error retained, decoder released");

    // Short looping clips cross many decoder/loop boundaries during one fade.
    const auto short_pcm = Decode(two);
    TransitionStream loops(two, {50, 1, true});
    loops.Begin(two, {51, 1, true}, 24001, media::CrossfadeCurve::kLinear);
    produced = 0;
    while (produced < 24001) {
        const auto block = loops.Read();
        Require(block && block->identity_ == StreamIdentity{50, produced / 9600} &&
                        block->first_sample_ == produced % 9600,
                "fade splits at loop boundaries");
        for (std::size_t index = 0; index < block->samples_.size(); ++index)
            Require(std::abs(block->samples_[index] - short_pcm[(produced % 9600) * 2 + index]) <
                            1e-6,
                    "correlated looping sources preserve PCM");
        produced += block->samples_.size() / 2;
    }
    const auto loop_next = loops.Read();
    Require(loop_next && loop_next->identity_ == StreamIdentity{51, 2} &&
                    loop_next->first_sample_ == 4801 && loops.PeakCursors() == 4,
            "incoming loop iteration and exact position preserved at handoff");

    TransitionStream short_source(two, {60});
    short_source.Begin(two, {61}, 24001, media::CrossfadeCurve::kLinear);
    produced = 0;
    while (produced < 24001) {
        const auto block = short_source.Read();
        Require(block.has_value(), "short sources pad only during remaining fade");
        if (produced >= 9600)
            for (float sample : block->samples_) Require(sample == 0, "EOF fade tail is silence");
        produced += block->samples_.size() / 2;
    }
    Require(!short_source.Read() && short_source.Progress().state_ == FadeState::kSubmitted,
            "short next source ends after exact fade duration");
}
}  // namespace
int main(int argc, char** argv) {
    try {
        Require(argc == 2, "fixture directory required");
        Run(std::filesystem::path(argv[1]));
        std::cout << "dual PCM: rate/block alignment, exact handoff, gain, cancellation, late "
                     "failure, "
                     "future cursor budget, looping and short EOF passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
