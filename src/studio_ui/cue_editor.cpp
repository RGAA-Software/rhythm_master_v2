#include "cue_editor.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>

#include "rhythm/graph/controls.h"

namespace rhythm::studio {
CueEdit CueEditor::Draw(const graph::Document& document, double playhead, double duration,
                        double bpm, const std::map<std::string, std::string>& text) {
    CueEdit edit;
    if (document.control_snapshots_.empty()) return edit;
    const auto label = [&](const std::string& key) {
        const auto found = text.find(key);
        return (found == text.end() ? key : found->second) + "###" + key;
    };
    if (!ImGui::CollapsingHeader(label("cue.title").c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        return edit;
    ImGui::PushID("cue_editor");
    const auto& snapshots = document.control_snapshots_;
    const auto select_snapshot = [&](std::uint64_t& value, const std::string& key) {
        auto found = std::find_if(snapshots.begin(), snapshots.end(),
                                  [&](const auto& item) { return item.id_ == value; });
        if (found == snapshots.end()) {
            value = snapshots.front().id_;
            found = snapshots.begin();
        }
        bool changed = false;
        if (ImGui::BeginCombo(label(key).c_str(), found->title_.c_str())) {
            for (const auto& item : snapshots) {
                const auto name = item.title_ + "###snapshot." + std::to_string(item.id_);
                if (ImGui::Selectable(name.c_str(), item.id_ == value)) {
                    value = item.id_;
                    changed = true;
                }
            }
            ImGui::EndCombo();
        }
        return changed;
    };
    auto cues = document.control_cues_;
    const auto publish = [&](bool commit) {
        try {
            auto ordered = cues;
            std::sort(ordered.begin(), ordered.end(),
                      [](const auto& a, const auto& b) { return a.seconds_ < b.seconds_; });
            (void)parameters::ControlSequence(graph::DescribeControls(document), ordered);
            edit.cues_ = std::move(ordered);
            edit.committed_ = commit;
            error_.clear();
        } catch (const std::exception&) {
            error_ = label("cue.invalid");
        }
    };
    select_snapshot(snapshot_, "cue.snapshot");
    ImGui::Checkbox(label("cue.snap").c_str(), &snap_);
    const auto quantize = [&](double seconds) {
        const auto step = 60.0 / std::clamp(bpm, 1.0, 999.0);
        return snap_ ? std::round(seconds / step) * step : seconds;
    };
    ImGui::BeginDisabled(cues.size() >= parameters::ControlSequence::kMaximumCues);
    if (ImGui::Button(label("cue.add").c_str())) {
        std::uint64_t id = 1;
        while (std::any_of(cues.begin(), cues.end(),
                           [&](const auto& cue) { return cue.id_ == id; }))
            ++id;
        const auto target = std::find_if(snapshots.begin(), snapshots.end(),
                                         [&](const auto& value) { return value.id_ == snapshot_; });
        cues.push_back(
                {id, target->title_, quantize(std::clamp(playhead, 0.0, 1e9)), snapshot_, 1, true});
        selected_ = id;
        publish(true);
    }
    ImGui::EndDisabled();
    const auto origin = ImGui::GetCursorScreenPos();
    const float width = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    const double range = std::max(0.01, duration);
    ImGui::Dummy({width, 88});
    ImGui::GetWindowDrawList()->AddRectFilled(origin, {origin.x + width, origin.y + 88},
                                              IM_COL32(22, 30, 40, 255));
    const auto position = [&](double seconds) {
        return origin.x + static_cast<float>(std::clamp(seconds / range, 0.0, 1.0)) * width;
    };
    for (std::size_t i = 0; i < cues.size(); ++i) {
        auto& cue = cues[i];
        const float y = origin.y + 5 + static_cast<float>(i % 3) * 26;
        const float x = position(cue.seconds_);
        const float end =
                std::min(origin.x + width, std::max(x + 12, position(cue.seconds_ + cue.fade_)));
        const auto color =
                cue.id_ == selected_ ? IM_COL32(235, 183, 61, 255) : IM_COL32(64, 158, 184, 255);
        ImGui::GetWindowDrawList()->AddRectFilled({x, y}, {end, y + 22}, color, 3);
        ImGui::SetCursorScreenPos({std::min(x, origin.x + width - 12), y});
        const auto name = "cue." + std::to_string(cue.id_);
        ImGui::InvisibleButton(name.c_str(), {std::max(12.0f, end - x), 22});
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s: %.3f s", cue.title_.c_str(), cue.seconds_);
        if (ImGui::IsItemActivated()) {
            selected_ = dragged_ = cue.id_;
            anchor_ = cue.seconds_;
            mouse_anchor_ = ImGui::GetIO().MousePos.x;
        }
        if (dragged_ == cue.id_ && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            const auto lower = i == 0 ? 0.0 : std::nextafter(cues[i - 1].seconds_, 1e9);
            const auto upper = i + 1 == cues.size() ? 1e9 - cue.fade_
                                                    : std::nextafter(cues[i + 1].seconds_, 0.0);
            cue.seconds_ = std::clamp(
                    quantize(anchor_ + (ImGui::GetIO().MousePos.x - mouse_anchor_) / width * range),
                    lower, upper);
            publish(false);
        }
    }
    if (dragged_ && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        if (ImGui::GetMouseDragDelta(ImGuiMouseButton_Left).x != 0) edit.committed_ = true;
        dragged_ = 0;
    }
    const auto head = position(playhead);
    ImGui::GetWindowDrawList()->AddLine({head, origin.y}, {head, origin.y + 88},
                                        IM_COL32(245, 245, 245, 255));
    ImGui::SetCursorScreenPos({origin.x, origin.y + 94});
    if (!error_.empty()) ImGui::TextWrapped("%s", error_.substr(0, error_.find("###")).c_str());
    const auto found = std::find_if(cues.begin(), cues.end(),
                                    [&](const auto& cue) { return cue.id_ == selected_; });
    if (found != cues.end()) {
        std::array<char, 129> title{};
        std::copy(found->title_.begin(), found->title_.end(), title.begin());
        if (ImGui::InputText(label("cue.name").c_str(), title.data(), title.size(),
                             ImGuiInputTextFlags_EnterReturnsTrue)) {
            found->title_ = title.data();
            publish(true);
        }
        const double zero = 0, maximum = 1e9;
        if (ImGui::DragScalar(label("cue.start").c_str(), ImGuiDataType_Double, &found->seconds_,
                              0.05f, &zero, &maximum, "%.3f s", ImGuiSliderFlags_AlwaysClamp))
            publish(false);
        edit.committed_ |= ImGui::IsItemDeactivatedAfterEdit();
        if (ImGui::DragScalar(label("cue.fade").c_str(), ImGuiDataType_Double, &found->fade_, 0.05f,
                              &zero, &maximum, "%.3f s", ImGuiSliderFlags_AlwaysClamp))
            publish(false);
        edit.committed_ |= ImGui::IsItemDeactivatedAfterEdit();
        if (ImGui::Checkbox(label("cue.smooth").c_str(), &found->smooth_)) publish(true);
        if (select_snapshot(found->snapshot_, "cue.target")) publish(true);
        if (ImGui::Button(label("cue.remove").c_str())) {
            std::erase_if(cues, [&](const auto& cue) { return cue.id_ == selected_; });
            selected_ = 0;
            publish(true);
        }
    }
    ImGui::PopID();
    return edit;
}
}  // namespace rhythm::studio
