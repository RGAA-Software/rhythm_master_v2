#include <stdexcept>

#include "rhythm/assets/store.h"
#include "rhythm/prepared_assets/prepare.h"

#ifdef RHYTHM_HAS_IMAGE_DECODER
#include "rhythm/media/audio_decoder.h"
#endif

namespace rhythm::prepared_assets {
std::optional<media::SoundtrackSource> PrepareSoundtrack(const project::RuntimePackage& package,
                                                         std::stop_token stop) {
    if (!package.streamed_audio_) {
        if (package.profile_ == project::PackageProfile::kMusicPerformanceV2)
            throw std::invalid_argument("project.soundtrack_invalid");
        return PrepareSoundtrack(package.soundtrack_, package.assets_, stop);
    }
    if (stop.stop_requested()) throw std::runtime_error("audio.canceled");
#ifdef RHYTHM_HAS_IMAGE_DECODER
    const auto& audio = *package.streamed_audio_;
    if (package.profile_ != project::PackageProfile::kMusicPerformanceV2 || !package.soundtrack_ ||
        !media::ValidSoundtrack(*package.soundtrack_, std::span(&audio.record_, 1)) ||
        audio.record_.bytes_ > project::kMaximumMusicAssetBytes ||
        !assets::VerifyFile(audio.record_, audio.bytes_, stop))
        throw std::invalid_argument("project.soundtrack_invalid");
    media::AudioDecoder decoder(audio.bytes_, 1, stop);
    if (!decoder.Read(stop)) throw std::invalid_argument("audio.empty_source");
    return media::SoundtrackSource{*package.soundtrack_, {}, audio.bytes_};
#else
    throw std::runtime_error("audio.decoder_unavailable");
#endif
}
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
