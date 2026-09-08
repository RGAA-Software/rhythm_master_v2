#include "timeline_panel.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace rhythm::studio {
std::size_t TimelinePanel::WaveformBins() const {
#ifdef RHYTHM_HAS_LOCAL_MEDIA
    return waveform_.BinCount();
#else
    return 0;
#endif
}
void TimelinePanel::CancelMediaPreview() {
#ifdef RHYTHM_HAS_LOCAL_MEDIA
    waveform_.Clear();
#endif
}
void TimelinePanel::Restart() {
    clock_.Seek(0);
    command_.seek_ = 0;
}
runtime::PlaybackCommand TimelinePanel::TakePlaybackCommand() {
    return std::exchange(command_, {});
}
double TimelinePanel::Advance(double host_seconds, bool,
                              const std::optional<runtime::PlaybackSample>& source) {
    const auto seconds = clock_.Advance(host_seconds, false, source);
    if (source && source->duration_) duration_ = *source->duration_;
    if (loop_ && !source && seconds >= duration_) {
        clock_.Seek(std::fmod(seconds, duration_));
    }
    return clock_.Seconds();
}
TimelineEdit TimelinePanel::Draw(const editor::Snapshot& base, bool seekable,
                                 const std::map<std::string, std::string>& text,
                                 const std::optional<std::filesystem::path>& music,
                                 const std::function<graph::NodeId()>& reserve_id) {
    if (ImGui::Button(text.at(Paused() ? "timeline.play" : "timeline.pause").c_str())) {
        clock_.SetPaused(!Paused());
        command_.paused_ = Paused();
    }
    ImGui::SameLine();
    if (ImGui::Button(text.at("timeline.restart").c_str())) Restart();
    ImGui::SameLine();
    ImGui::BeginDisabled(clock_.FollowingMedia());
    ImGui::Checkbox(text.at("timeline.loop").c_str(), &loop_);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(110);
    const std::array labels{text.at("timeline.seconds"), text.at("timeline.frames"),
                            text.at("timeline.beats")};
    if (ImGui::BeginCombo("###timeline.unit", labels[unit_].c_str())) {
        for (int index = 0; index < 3; ++index)
            if (ImGui::Selectable(labels[index].c_str(), unit_ == index)) unit_ = index;
        ImGui::EndCombo();
    }
    if (unit_ > 0) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120);
        const double minimum = 1, maximum = unit_ == 1 ? 240 : 400;
        auto& rate = unit_ == 1 ? fps_ : bpm_;
        ImGui::DragScalar(unit_ == 1 ? "FPS" : "BPM", ImGuiDataType_Double, &rate, 1, &minimum,
                          &maximum, "%.1f", ImGuiSliderFlags_AlwaysClamp);
        rate = std::isfinite(rate) ? std::clamp(rate, minimum, maximum) : minimum;
    }
    const auto factor = unit_ == 0 ? 1.0 : unit_ == 1 ? fps_ : bpm_ / 60;
    auto position = clock_.Seconds() * factor;
    const double minimum = 0, maximum = duration_ * factor;
    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderScalar("###timeline.position", ImGuiDataType_Double, &position, &minimum,
                            &maximum, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
        if (std::isfinite(position)) {
            if (unit_ == 1) position = std::round(position);
            clock_.Seek(std::clamp(position / factor, 0.0, duration_));
            command_.seek_ = clock_.Seconds();
        }
    }
    ImGui::SetNextItemWidth(150);
    ImGui::BeginDisabled(clock_.FollowingMedia());
    const double duration_minimum = 0.01, duration_maximum = 86400;
    ImGui::DragScalar(text.at("timeline.duration").c_str(), ImGuiDataType_Double, &duration_, 0.1f,
                      &duration_minimum, &duration_maximum, "%.2f s", ImGuiSliderFlags_AlwaysClamp);
    duration_ = std::isfinite(duration_) ? std::clamp(duration_, duration_minimum, duration_maximum)
                                         : 10;
    ImGui::EndDisabled();
    if (clock_.FollowingMedia()) ImGui::TextWrapped("%s", text.at("timeline.media_clock").c_str());
    if (!seekable) ImGui::TextWrapped("%s", text.at("timeline.stateful").c_str());
#ifdef RHYTHM_HAS_LOCAL_MEDIA
    if (const auto seek = waveform_.Draw(music, clock_.Seconds(), text)) {
        clock_.Seek(*seek);
        command_.seek_ = *seek;
    }
#else
    (void)music;
#endif
    return tracks_.Draw(base, clock_.Seconds(), duration_, text, reserve_id, "timeline.add_section",
                        bpm_);
}
}  // namespace rhythm::studio
