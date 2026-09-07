#pragma once

#include <cmath>
#include <memory>
#include <span>
#include <vector>

#include "rhythm/assets/types.h"

namespace rhythm::media {
// Authored playback settings reference immutable content, never a host path.
// One track starts at performance time zero. Looping uses the shared playback
// service's bounded repeat semantics; this is not a multitrack arrangement.
struct Soundtrack {
    assets::AssetId asset_{};
    std::string title_{};
    float gain_ = 1;
    bool loop_ = false;
    bool operator==(const Soundtrack&) const = default;
};
struct SoundtrackSource {
    Soundtrack binding_{};
    // Shared by package/session and the asynchronous decoder; no host path.
    std::shared_ptr<const std::vector<std::uint8_t>> bytes_{};
};
inline bool ValidSoundtrack(const Soundtrack& track, std::span<const assets::AssetRecord> records) {
    return assets::ValidId(track.asset_) && track.title_.size() <= 512 &&
           track.title_.find('\0') == std::string::npos && std::isfinite(track.gain_) &&
           track.gain_ >= 0 && track.gain_ <= 1 &&
           std::any_of(records.begin(), records.end(), [&](const auto& record) {
               return record.id_ == track.asset_ && record.bytes_ > 0 &&
                      record.media_type_.starts_with("audio/");
           });
}
}  // namespace rhythm::media
