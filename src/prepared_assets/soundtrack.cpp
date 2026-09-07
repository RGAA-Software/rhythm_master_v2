#include <stdexcept>

#include "rhythm/prepared_assets/prepare.h"

#ifdef RHYTHM_HAS_IMAGE_DECODER
#include "rhythm/media/audio_decoder.h"
#endif

namespace rhythm::prepared_assets {
std::optional<media::SoundtrackSource> PrepareSoundtrack(
        const std::optional<media::Soundtrack>& binding,
        std::span<const project::PackagedAsset> assets, std::stop_token stop) {
    if (!binding) return {};
    if (stop.stop_requested()) throw std::runtime_error("audio.canceled");
#ifdef RHYTHM_HAS_IMAGE_DECODER
    for (const auto& asset : assets) {
        if (asset.record_.id_ != binding->asset_) continue;
        if (!media::ValidSoundtrack(*binding, std::span(&asset.record_, 1)) ||
            asset.bytes_.size() != asset.record_.bytes_ ||
            asset.bytes_.size() > project::kMaximumPackageAssetBytes)
            throw std::invalid_argument("project.soundtrack_invalid");
        auto bytes = std::make_shared<const std::vector<std::uint8_t>>(asset.bytes_.begin(),
                                                                       asset.bytes_.end());
        media::AudioDecoder decoder(bytes, 1, stop);
        if (!decoder.Read(stop)) throw std::invalid_argument("audio.empty_source");
        return media::SoundtrackSource{*binding, std::move(bytes)};
    }
    throw std::invalid_argument("project.soundtrack_invalid");
#else
    (void)assets;
    throw std::runtime_error("audio.decoder_unavailable");
#endif
}
}  // namespace rhythm::prepared_assets
