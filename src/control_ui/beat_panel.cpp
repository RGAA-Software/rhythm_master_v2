#include "rhythm/control_ui/beat_panel.h"

#include <imgui.h>

#include <array>

namespace rhythm::control_ui {
namespace {
std::string Text(const std::map<std::string, std::string>& text, const std::string& key) {
    const auto found = text.find(key);
    return found == text.end() ? key : found->second;
}
std::string Label(const std::map<std::string, std::string>& text, const std::string& key) {
    return Text(text, key) + "###" + key;
}
}  // namespace
BeatEdit BeatPanel::Draw(const std::optional<parameters::BeatSettings>& grid, double seconds,
                         const std::map<std::string, std::string>& text) {
    BeatEdit edit{false, grid};
    if (!grid) mode_ = parameters::Quantization::kImmediate;
    if (!ImGui::CollapsingHeader(Label(text, "beat.title").c_str())) return edit;
    bool enabled = grid.has_value();
    if (ImGui::Checkbox(Label(text, "beat.enabled").c_str(), &enabled)) {
        edit.changed_ = true;
        edit.grid_ = enabled ? std::optional(parameters::BeatSettings{}) : std::nullopt;
        taps_.Reset();
    }
    if (!edit.grid_) {
        mode_ = parameters::Quantization::kImmediate;
        ImGui::TextWrapped("%s", Text(text, "beat.disabled").c_str());
        return edit;
    }
    auto settings = *edit.grid_;
    ImGui::SetNextItemWidth(160);
    edit.changed_ |= ImGui::InputDouble(Label(text, "beat.bpm").c_str(), &settings.bpm_, 0, 0,
                                        "%.2f", ImGuiInputTextFlags_EnterReturnsTrue);
    int beats = static_cast<int>(settings.beats_per_bar_);
    ImGui::SetNextItemWidth(160);
    if (ImGui::InputInt(Label(text, "beat.numerator").c_str(), &beats, 0, 0,
                        ImGuiInputTextFlags_EnterReturnsTrue) &&
        beats >= 1 && beats <= 32) {
        settings.beats_per_bar_ = static_cast<std::uint32_t>(beats);
        edit.changed_ = true;
    }
    ImGui::SetNextItemWidth(160);
    if (ImGui::BeginCombo(Label(text, "beat.denominator").c_str(),
                          std::to_string(settings.beat_unit_).c_str())) {
        for (const std::uint32_t unit : {1u, 2u, 4u, 8u, 16u, 32u})
            if (ImGui::Selectable(std::to_string(unit).c_str(), settings.beat_unit_ == unit)) {
                settings.beat_unit_ = unit;
                taps_.Reset();
                edit.changed_ = true;
            }
        ImGui::EndCombo();
    }
    ImGui::SetNextItemWidth(160);
    edit.changed_ |=
            ImGui::InputDouble(Label(text, "beat.origin").c_str(), &settings.origin_seconds_, 0, 0,
                               "%.4f s", ImGuiInputTextFlags_EnterReturnsTrue);
    if (ImGui::Button(Label(text, "beat.mark_origin").c_str())) {
        settings.origin_seconds_ = seconds;
        edit.changed_ = true;
    }
    ImGui::SameLine();
    if (ImGui::Button(Label(text, "beat.tap").c_str()))
        if (const auto bpm = taps_.Tap(ImGui::GetTime(), settings.beat_unit_)) {
            settings.bpm_ = *bpm;
            edit.changed_ = true;
        }
    if (taps_.Count()) ImGui::Text("%s %zu / 4", Text(text, "beat.taps").c_str(), taps_.Count());
    if (!parameters::ValidBeatSettings(settings)) {
        ImGui::TextWrapped("%s", Text(text, "beat.invalid").c_str());
        edit.changed_ = false;
        return edit;
    }
    edit.grid_ = settings;
    constexpr std::array kModes{"beat.immediate", "beat.next_beat", "beat.next_bar"};
    ImGui::SetNextItemWidth(160);
    if (ImGui::BeginCombo(Label(text, "beat.quantization").c_str(),
                          Text(text, kModes.at(static_cast<std::size_t>(mode_))).c_str())) {
        for (std::size_t index = 0; index < kModes.size(); ++index)
            if (ImGui::Selectable(Text(text, kModes[index]).c_str(),
                                  index == static_cast<std::size_t>(mode_)))
                mode_ = static_cast<parameters::Quantization>(index);
        ImGui::EndCombo();
    }
    const auto position = parameters::BeatGrid(settings).Position(seconds);
    ImGui::Text("%s %lld / %u", Text(text, "beat.position").c_str(),
                static_cast<long long>(position.bar_ + 1), position.beat_in_bar_ + 1);
    ImGui::TextWrapped("%s", Text(text, "beat.manual_help").c_str());
    return edit;
}
bool DrawPerformanceAction(const player::PerformanceAction& action,
                           const std::map<std::string, std::string>& text) {
    using State = player::PerformanceActionState;
    if (action.state_ == State::kIdle) return false;
    constexpr std::array kStates{"beat.idle",      "beat.pending",   "beat.dispatched",
                                 "beat.completed", "beat.cancelled", "beat.failed"};
    constexpr std::array kReasons{
            "beat.none",    "beat.user_cancelled", "beat.source_changed",    "beat.grid_changed",
            "beat.no_grid", "beat.no_boundary",    "beat.target_unavailable"};
    ImGui::Text(
            "%s #%llu: %s",
            Text(text, action.kind_ == player::PerformanceActionKind::kSnapshot ? "beat.snapshot"
                                                                                : "beat.scene")
                    .c_str(),
            static_cast<unsigned long long>(action.target_),
            Text(text, kStates.at(static_cast<std::size_t>(action.state_))).c_str());
    if (action.reason_ != player::PerformanceActionReason::kNone)
        ImGui::TextWrapped(
                "%s", Text(text, kReasons.at(static_cast<std::size_t>(action.reason_))).c_str());
    if (action.state_ != State::kPending && action.state_ != State::kDispatched) return false;
    if (action.due_seconds_) {
        ImGui::SameLine();
        ImGui::Text("@ %.3f s", *action.due_seconds_);
    }
    const auto cancel = Text(text, "beat.cancel") + "###beat.cancel." + std::to_string(action.id_);
    return ImGui::Button(cancel.c_str());
}
}  // namespace rhythm::control_ui
