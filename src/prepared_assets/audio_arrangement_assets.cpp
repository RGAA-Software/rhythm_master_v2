#include "audio_arrangement_assets.h"

#include <cmath>
#include <map>
#include <stdexcept>

#ifdef RHYTHM_HAS_IMAGE_DECODER
#include "rhythm/media/audio_decoder.h"
#endif

namespace rhythm::prepared_assets::detail {
media::SoundtrackSource PrepareArrangement(const media::Soundtrack& binding,
                                           std::span<const project::PackagedAsset> assets,
                                           std::stop_token stop) {
    if (stop.stop_requested()) throw std::runtime_error("audio.canceled");
#ifdef RHYTHM_HAS_IMAGE_DECODER
    if (assets.size() > project::kMaximumPackageAssets)
        throw std::length_error("package.asset_count");
    std::vector<assets::AssetRecord> records;
    for (const auto& asset : assets) records.push_back(asset.record_);
    if (binding.clips_.empty() || !media::ValidSoundtrack(binding, records))
        throw std::invalid_argument("project.soundtrack_invalid");
    media::AudioArrangementSource source{media::AudioArrangement(binding.clips_), {}};
    std::map<std::string, std::optional<double>> durations;
    std::size_t total = 0;
    for (const auto& clip : binding.clips_) {
        if (stop.stop_requested()) throw std::runtime_error("audio.canceled");
        if (!durations.contains(clip.asset_.sha256_)) {
            const auto found = std::find_if(assets.begin(), assets.end(), [&](const auto& asset) {
                return asset.record_.id_ == clip.asset_;
            });
            if (found == assets.end() || found->bytes_.size() != found->record_.bytes_ ||
                found->bytes_.size() > project::kMaximumPackageAssetBytes - total)
                throw std::invalid_argument("project.soundtrack_invalid");
            total += found->bytes_.size();
            auto bytes = std::make_shared<const std::vector<std::uint8_t>>(found->bytes_.begin(),
                                                                           found->bytes_.end());
            media::AudioDecoder decoder(bytes, 1, stop);
            if (!decoder.Read(stop)) throw std::invalid_argument("audio.empty_source");
            durations.emplace(clip.asset_.sha256_, decoder.Info().duration_seconds_);
            source.assets_.push_back({clip.asset_, std::move(bytes), {}});
        }
        const auto duration = durations.at(clip.asset_.sha256_);
        if (duration && std::isfinite(*duration) &&
            clip.timing_.source_out_ > *duration + 1.0 / media::kAudioSampleRate)
            throw std::invalid_argument("audio.clip_source_range");
    }
    return {binding, {}, {}, std::move(source)};
#else
    static_cast<void>(binding);
    static_cast<void>(assets);
    throw std::runtime_error("audio.decoder_unavailable");
#endif
}
}  // namespace rhythm::prepared_assets::detail
