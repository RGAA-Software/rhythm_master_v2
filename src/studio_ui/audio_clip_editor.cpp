#include "audio_clip_editor.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>

#ifdef RHYTHM_HAS_LOCAL_MEDIA
#include "rhythm/media/waveform_index.h"
#endif

namespace rhythm::studio {
void AudioClipEditor::Reset() {
    drag_.reset();
    property_active_ = false;
    selected_ = 0;
    error_.clear();
}
AudioClipEdit AudioClipEditor::Draw(const std::vector<media::AudioClip>& clips, double playhead,
                                    double duration, double bpm,
                                    const std::map<std::string, std::string>& text,
                                    const ClipWaveforms& waveforms) {
#ifndef RHYTHM_HAS_LOCAL_MEDIA
    (void)waveforms;
#endif
    AudioClipEdit edit;
    if (clips.empty()) {
        Reset();
        return edit;
    }
    const auto label = [&](const std::string& key) { return text.at(key) + "###" + key; };
    const auto accept = [&](std::vector<media::AudioClip> candidate) {
        try {
            if (!candidate.empty()) (void)media::AudioArrangement(candidate);
            edit.clips_ = std::move(candidate);
            error_.clear();
            return true;
        } catch (const std::exception&) {
            error_ = "audio.arrangement_invalid";
            return false;
        }
    };
    const auto replace = [&](const media::AudioClip& clip) {
        auto next = clips;
        for (auto& item : next)
            if (item.id_ == clip.id_) item = clip;
        return accept(std::move(next));
    };
    const auto find = [&](std::uint64_t id) {
        return std::find_if(clips.begin(), clips.end(),
                            [&](const auto& clip) { return clip.id_ == id; });
    };
    if (find(selected_) == clips.end()) selected_ = clips.front().id_;
    ImGui::TextWrapped("%s", text.at("audio.arrangement_help").c_str());
    ImGui::Checkbox(label("audio.clip_snap").c_str(), &snap_);
    const auto snap = [&](double seconds) {
        return snap_ && std::isfinite(bpm) && bpm > 0 ? std::round(seconds * bpm / 60) * 60 / bpm
                                                      : seconds;
    };
    const auto range = std::clamp(std::isfinite(duration) ? duration : 10.0, 0.01, 604800.0);
    const float row_height = 54 + ImGui::GetStyle().ItemSpacing.y;
    ImGui::BeginDisabled(property_active_);
    if (ImGui::BeginChild("###audio.clip_rows",
                          {0, std::min(180.0F, 24 + clips.size() * row_height)},
                          ImGuiChildFlags_Borders)) {
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(clips.size()), row_height);
        while (clipper.Step()) {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                const auto& clip = clips[static_cast<std::size_t>(row)];
                ImGui::PushID(std::to_string(clip.id_).c_str());
                const auto origin = ImGui::GetCursorScreenPos();
                const float width = std::max(1.0F, ImGui::GetContentRegionAvail().x);
                ImGui::InvisibleButton("###audio.clip_bar", {width, 54});
                const auto x = [&](double seconds) {
                    return origin.x +
                           static_cast<float>(std::clamp(seconds / range, 0.0, 1.0)) * width;
                };
                const auto left = x(clip.timing_.start_);
                const auto right = x(clip.timing_.start_ + clip.timing_.duration_);
                if (ImGui::IsItemActivated()) {
                    selected_ = clip.id_;
                    const auto mouse = ImGui::GetIO().MousePos.x;
                    if (mouse >= left - 5 && mouse <= right + 5) {
                        const int mode = right - left < 12              ? 0
                                         : std::abs(mouse - left) <= 6  ? -1
                                         : std::abs(mouse - right) <= 6 ? 1
                                                                        : 0;
                        drag_ = Drag{clip, mouse, mode};
                    }
                }
                if (drag_ && drag_->origin_.id_ == clip.id_) {
                    if (ImGui::IsItemActive()) {
                        auto next = drag_->origin_;
                        auto& timing = next.timing_;
                        const auto delta =
                                (ImGui::GetIO().MousePos.x - drag_->mouse_x_) / width * range;
                        const auto end = timing.start_ + timing.duration_;
                        if (drag_->mode_ == 0)
                            timing.start_ = std::clamp(snap(timing.start_ + delta), 0.0,
                                                       604800 - timing.duration_);
                        else if (drag_->mode_ < 0) {
                            timing.start_ =
                                    std::clamp(snap(timing.start_ + delta), 0.0, end - 0.001);
                            timing.duration_ = end - timing.start_;
                        } else {
                            timing.duration_ = std::clamp(snap(end + delta) - timing.start_, 0.001,
                                                          604800 - timing.start_);
                        }
                        if (next != clip && replace(next)) drag_->changed_ = true;
                    }
                    if (ImGui::IsItemDeactivated()) {
                        edit.committed_ = drag_->changed_;
                        drag_.reset();
                    }
                }
                if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                auto& draw = *ImGui::GetWindowDrawList();
                draw.AddRectFilled(origin, {origin.x + width, origin.y + 50},
                                   ImGui::GetColorU32(ImGuiCol_FrameBg), 3);
                draw.AddRectFilled(
                        {left, origin.y + 2}, {std::max(left + 1, right), origin.y + 48},
                        ImGui::GetColorU32(clip.muted_             ? ImGuiCol_FrameBgHovered
                                           : selected_ == clip.id_ ? ImGuiCol_SliderGrabActive
                                                                   : ImGuiCol_SliderGrab),
                        3);
#ifdef RHYTHM_HAS_LOCAL_MEDIA
                if (const auto found = waveforms.find(clip.asset_.sha256_);
                    found != waveforms.end() && found->second && right > left) {
                    const parameters::ClipInterval interval(clip.timing_);
                    const auto& waveform = *found->second;
                    const int columns =
                            std::clamp(static_cast<int>(std::ceil(right - left)), 1, 1024);
                    draw.PushClipRect({left, origin.y + 22}, {right, origin.y + 48}, true);
                    for (int column = 0; column < columns; ++column) {
                        const auto begin_x = left + (right - left) * column / columns;
                        const auto end_x = left + (right - left) * (column + 1) / columns;
                        const auto begin = (begin_x - origin.x) / width * range;
                        const auto end = (end_x - origin.x) / width * range;
                        const auto peak = media::ClipWaveformPeak(waveform, interval, begin, end);
                        const auto gain =
                                static_cast<float>(interval.Sample((begin + end) / 2).gain_) *
                                clip.gain_;
                        const auto middle = (begin_x + end_x) / 2;
                        draw.AddLine({middle, origin.y + 35 - peak.maximum_ * gain * 12},
                                     {middle, origin.y + 35 - peak.minimum_ * gain * 12},
                                     ImGui::GetColorU32(clip.muted_ ? ImGuiCol_TextDisabled
                                                                    : ImGuiCol_PlotLines));
                    }
                    draw.PopClipRect();
                }
#endif
                for (const auto edge : {left, right})
                    draw.AddLine({edge, origin.y + 4}, {edge, origin.y + 46},
                                 ImGui::GetColorU32(ImGuiCol_Text));
                const auto cursor = x(playhead);
                draw.AddLine({cursor, origin.y}, {cursor, origin.y + 50},
                             ImGui::GetColorU32(ImGuiCol_PlotLines));
                draw.AddText({origin.x + 6, origin.y + 6}, ImGui::GetColorU32(ImGuiCol_Text),
                             clip.title_.c_str());
                ImGui::PopID();
            }
        }
    }
    ImGui::EndChild();
    ImGui::EndDisabled();
    if (drag_ && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        edit.committed_ = drag_->changed_;
        drag_.reset();
    }
    if (edit.clips_ || edit.committed_) return edit;
    ImGui::BeginDisabled(drag_.has_value() || property_active_);
    if (ImGui::Button(label("audio.clip_duplicate").c_str())) {
        auto next = clips;
        auto duplicate = *find(selected_);
        std::uint64_t id = 0;
        for (const auto& clip : clips) id = std::max(id, clip.id_);
        if (id != std::numeric_limits<std::uint64_t>::max()) {
            duplicate.id_ = id + 1;
            duplicate.timing_.start_ =
                    std::clamp(snap(playhead), 0.0, 604800 - duplicate.timing_.duration_);
            next.push_back(duplicate);
            if (accept(std::move(next))) {
                selected_ = duplicate.id_;
                edit.committed_ = true;
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(label("audio.clip_remove").c_str())) {
        auto next = clips;
        std::erase_if(next, [&](const auto& clip) { return clip.id_ == selected_; });
        if (accept(std::move(next))) edit.committed_ = true;
    }
    ImGui::EndDisabled();
    if (edit.clips_) return edit;
    auto clip = *find(selected_);
    ImGui::PushID("audio.clip_properties");
    ImGui::PushID(std::to_string(selected_).c_str());
    ImGui::BeginDisabled(drag_.has_value());
    property_active_ = false;
    bool changed = false;
    const auto activity = [&] {
        property_active_ |= ImGui::IsItemActive();
        edit.committed_ |= ImGui::IsItemDeactivatedAfterEdit();
    };
    std::array<char, 129> title{};
    std::memcpy(title.data(), clip.title_.data(), std::min(clip.title_.size(), title.size() - 1));
    if (ImGui::InputText(label("audio.clip_title").c_str(), title.data(), title.size())) {
        clip.title_ = title.data();
        changed = true;
    }
    activity();
    const auto number = [&](const char* key, double& value, double minimum, double maximum) {
        ImGui::SetNextItemWidth(150);
        changed |= ImGui::DragScalar(label(key).c_str(), ImGuiDataType_Double, &value, 0.02F,
                                     &minimum, &maximum, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        activity();
    };
    auto& timing = clip.timing_;
    number("clip_start", timing.start_, 0, 604800);
    number("clip_duration", timing.duration_, 0.001, 604800);
    number("source_in", timing.source_in_, 0, 604800);
    number("source_out", timing.source_out_, 0.001, 604800);
    number("fade_in", timing.fade_in_, 0, 604800);
    number("fade_out", timing.fade_out_, 0, 604800);
    changed |= ImGui::SliderFloat(label("audio.clip_gain").c_str(), &clip.gain_, 0, 1);
    activity();
    changed |= ImGui::SliderFloat(label("audio.clip_pan").c_str(), &clip.pan_, -1, 1);
    activity();
    const auto toggle = [&](const char* key, bool& value) {
        if (ImGui::Checkbox(label(key).c_str(), &value)) changed = edit.committed_ = true;
    };
    toggle("audio.clip_mute", clip.muted_);
    toggle("audio.clip_smooth", timing.smooth_);
    bool loop = timing.end_ == parameters::ClipEnd::kLoop;
    toggle("audio.clip_loop", loop);
    timing.end_ = loop ? parameters::ClipEnd::kLoop : parameters::ClipEnd::kBlank;
    if (changed) replace(clip);
    ImGui::EndDisabled();
    ImGui::PopID();
    ImGui::PopID();
    if (!error_.empty()) ImGui::TextWrapped("%s", text.at(error_).c_str());
    return edit;
}
}  // namespace rhythm::studio
