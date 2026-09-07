#pragma once

#include <nlohmann/json.hpp>
#include <stdexcept>

#include "rhythm/media/soundtrack.h"

namespace rhythm::project::detail {
inline nlohmann::json EncodeSoundtrack(const media::Soundtrack& track,
                                       std::span<const assets::AssetRecord> records) {
    if (!media::ValidSoundtrack(track, records))
        throw std::invalid_argument("project.soundtrack_invalid");
    return {{"sha256", track.asset_.sha256_},
            {"title", track.title_},
            {"gain", track.gain_},
            {"loop", track.loop_}};
}
inline media::Soundtrack DecodeSoundtrack(const nlohmann::json& value,
                                          std::span<const assets::AssetRecord> records) {
    if (!value.is_object() || value.size() != 4 || !value.at("gain").is_number() ||
        !value.at("loop").is_boolean())
        throw std::invalid_argument("project.soundtrack_invalid");
    media::Soundtrack track{{value.at("sha256").get<std::string>()},
                            value.at("title").get<std::string>(),
                            value.at("gain").get<float>(),
                            value.at("loop").get<bool>()};
    EncodeSoundtrack(track, records);
    return track;
}
}  // namespace rhythm::project::detail
