#pragma once

#include <nlohmann/json.hpp>
#include <stdexcept>

#include "rhythm/media/audio_arrangement.h"

namespace rhythm::project::detail {
inline nlohmann::json EncodeAudioClips(const std::vector<media::AudioClip>& clips) {
    (void)media::AudioArrangement(clips);
    auto result = nlohmann::json::array();
    for (const auto& clip : clips) {
        const auto& timing = clip.timing_;
        result.push_back({{"id", clip.id_},
                          {"title", clip.title_},
                          {"sha256", clip.asset_.sha256_},
                          {"start", timing.start_},
                          {"duration", timing.duration_},
                          {"source_in", timing.source_in_},
                          {"source_out", timing.source_out_},
                          {"loop", timing.end_ == parameters::ClipEnd::kLoop},
                          {"fade_in", timing.fade_in_},
                          {"fade_out", timing.fade_out_},
                          {"smooth", timing.smooth_},
                          {"gain", clip.gain_},
                          {"pan", clip.pan_},
                          {"muted", clip.muted_}});
    }
    return result;
}
inline std::vector<media::AudioClip> DecodeAudioClips(const nlohmann::json& records) {
    if (!records.is_array() || records.empty() ||
        records.size() > media::AudioArrangement::kMaximumClips)
        throw std::invalid_argument("audio.clip_budget");
    std::vector<media::AudioClip> result;
    for (const auto& value : records) {
        if (!value.is_object() || value.size() != 14 || !value.at("id").is_number_unsigned() ||
            !value.at("loop").is_boolean() || !value.at("smooth").is_boolean() ||
            !value.at("muted").is_boolean())
            throw std::invalid_argument("audio.clip_invalid");
        for (const auto key :
             {"start", "duration", "source_in", "source_out", "fade_in", "fade_out", "gain", "pan"})
            if (!value.at(key).is_number()) throw std::invalid_argument("audio.clip_invalid");
        media::AudioClip clip;
        clip.id_ = value.at("id").get<std::uint64_t>();
        clip.title_ = value.at("title").get<std::string>();
        clip.asset_.sha256_ = value.at("sha256").get<std::string>();
        clip.timing_ = {value.at("start").get<double>(),
                        value.at("duration").get<double>(),
                        value.at("source_in").get<double>(),
                        value.at("source_out").get<double>(),
                        1,
                        value.at("loop").get<bool>() ? parameters::ClipEnd::kLoop
                                                     : parameters::ClipEnd::kBlank,
                        value.at("fade_in").get<double>(),
                        value.at("fade_out").get<double>(),
                        value.at("smooth").get<bool>()};
        clip.gain_ = value.at("gain").get<float>();
        clip.pan_ = value.at("pan").get<float>();
        clip.muted_ = value.at("muted").get<bool>();
        result.push_back(std::move(clip));
    }
    (void)media::AudioArrangement(result);
    return result;
}
}  // namespace rhythm::project::detail
