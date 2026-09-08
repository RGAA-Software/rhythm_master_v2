#include "audio_clip_import.h"

#include <cmath>
#include <limits>
#include <stdexcept>

#include "rhythm/media/audio_decoder.h"

namespace rhythm::content::detail {
namespace {
double Duration(const assets::Store& store, const assets::AssetRecord& record,
                std::stop_token stop) {
    media::AudioDecoder decoder(store.Open(record, 256 * 1024 * 1024, stop), 1, stop);
    if (!decoder.Read(stop)) throw std::invalid_argument("audio.empty_source");
    const auto duration = decoder.Info().duration_seconds_;
    if (!duration || !std::isfinite(*duration) || *duration <= 0 ||
        *duration > parameters::ClipInterval::kMaximumSeconds)
        throw std::invalid_argument("audio.duration_required");
    return *duration;
}
std::string ClipTitle(std::string title) {
    if (title.size() > 128) {
        std::size_t end = 128;
        while (end && (static_cast<unsigned char>(title[end]) & 0xc0) == 0x80) --end;
        title.resize(end);
    }
    return title.empty() ? "Music" : title;
}
}  // namespace
media::Soundtrack AppendMusic(const editor::Snapshot& snapshot, const assets::AssetRecord& record,
                              std::string title, const assets::Store& store, float gain, bool loop,
                              std::stop_token stop) {
    auto binding = snapshot.soundtrack_.value_or(media::Soundtrack{record.id_, title, gain, loop});
    if (snapshot.soundtrack_ && binding.clips_.empty()) {
        const auto found =
                std::find_if(snapshot.assets_.begin(), snapshot.assets_.end(),
                             [&](const auto& asset) { return asset.id_ == binding.asset_; });
        if (found == snapshot.assets_.end())
            throw std::invalid_argument("project.soundtrack_invalid");
        const auto duration = Duration(store, *found, stop);
        binding.clips_.push_back(
                {1, ClipTitle(binding.title_), binding.asset_, {0, duration, 0, duration}});
    }
    std::uint64_t id = 0;
    double start = 0;
    for (const auto& clip : binding.clips_) {
        id = std::max(id, clip.id_);
        start = std::max(start, clip.timing_.start_ + clip.timing_.duration_);
    }
    if (id == std::numeric_limits<std::uint64_t>::max() ||
        binding.clips_.size() >= media::AudioArrangement::kMaximumClips)
        throw std::length_error("audio.clip_budget");
    const auto duration = Duration(store, record, stop);
    binding.clips_.push_back(
            {id + 1, ClipTitle(std::move(title)), record.id_, {start, duration, 0, duration}});
    binding.asset_ = binding.clips_.front().asset_;
    (void)media::AudioArrangement(binding.clips_);
    return binding;
}
}  // namespace rhythm::content::detail
