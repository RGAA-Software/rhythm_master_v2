#pragma once

#include <cmath>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "rhythm/assets/types.h"
#include "rhythm/media/audio_arrangement.h"
#include "rhythm/storage/file_bytes.h"

namespace rhythm::media {
// Authored playback settings reference immutable content, never a host path.
// Empty clips preserve the legacy single source at time zero. Otherwise clips
// define the complete mix; asset identifies its first source for authoring.
// Master gain/loop apply to either source through the same playback service.
struct Soundtrack {
    assets::AssetId asset_{};
    std::string title_{};
    float gain_ = 1;
    bool loop_ = false;
    std::vector<AudioClip> clips_{};
    bool operator==(const Soundtrack&) const = default;
};
struct SoundtrackSource {
    Soundtrack binding_{};
    // Shared by package/session and the asynchronous decoder; no host path.
    std::shared_ptr<const std::vector<std::uint8_t>> bytes_{};
    // Exactly one source is present. File-backed songs retain their validated
    // range across package replacement; only decoder workers perform reads.
    storage::FileBytes file_bytes_{};
    // An arranged mix replaces both legacy single-source fields above.
    std::optional<AudioArrangementSource> arrangement_{};
};
inline bool ValidSoundtrackSource(const SoundtrackSource& source) {
    const int count = static_cast<int>(bool(source.bytes_)) +
                      static_cast<int>(source.file_bytes_.Valid()) +
                      static_cast<int>(source.arrangement_.has_value());
    return count == 1 && (source.arrangement_ ? source.arrangement_->arrangement_.Clips() ==
                                                                source.binding_.clips_ &&
                                                        !source.binding_.clips_.empty()
                                              : source.binding_.clips_.empty());
}
inline bool ValidSoundtrack(const Soundtrack& track, std::span<const assets::AssetRecord> records) {
    const auto audio_record = [&](const assets::AssetId& id) {
        return std::any_of(records.begin(), records.end(), [&](const auto& record) {
            return record.id_ == id && record.bytes_ > 0 &&
                   record.media_type_.starts_with("audio/");
        });
    };
    const bool valid = assets::ValidId(track.asset_) && track.title_.size() <= 512 &&
                       track.title_.find('\0') == std::string::npos && std::isfinite(track.gain_) &&
                       track.gain_ >= 0 && track.gain_ <= 1 && audio_record(track.asset_);
    if (!valid || track.clips_.empty()) return valid;
    if (track.asset_ != track.clips_.front().asset_) return false;
    try {
        (void)AudioArrangement(track.clips_);
        return std::all_of(track.clips_.begin(), track.clips_.end(),
                           [&](const auto& clip) { return audio_record(clip.asset_); });
    } catch (const std::exception&) {
        return false;
    }
}
}  // namespace rhythm::media
