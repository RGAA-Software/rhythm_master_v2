#include "transform_automation.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <set>

#include "rhythm/runtime/runtime.h"

namespace rhythm::studio {
OutputEdit TransformAutomation::Draw(const editor::Snapshot& snapshot, graph::NodeId selected,
                                     std::span<const runtime::NodeOutput> outputs, bool current,
                                     bool gesture_active,
                                     const std::map<std::string, std::string>& text) {
    OutputEdit result;
    const auto message = [&](const std::string& key) -> std::string {
        const auto found = text.find(key);
        return found == text.end() ? key : found->second;
    };
    const auto label = [&](const std::string& key) { return message(key) + "###" + key; };
    if (document_id_ != snapshot.document_.id_ || revision_ != snapshot.document_.revision_ ||
        selected_ != selected) {
        document_id_ = snapshot.document_.id_;
        revision_ = snapshot.document_.revision_;
        selected_ = selected;
        drivers_.clear();
        desired_.clear();
        error_.clear();
        const auto inspected =
                editor::InspectTransformDrivers(snapshot.document_, graph::Registry{}, selected);
        if (std::holds_alternative<std::vector<editor::TransformDriver>>(inspected))
            drivers_ = std::get<std::vector<editor::TransformDriver>>(inspected);
    }
    if (drivers_.empty()) return result;
    ImGui::BeginDisabled(gesture_active);
    const auto expanded = ImGui::CollapsingHeader(label("transform_driver.title").c_str());
    ImGui::EndDisabled();
    if (!expanded) return result;
    editor::TransformSample sample{snapshot.document_.id_, snapshot.document_.revision_};
    std::set<graph::NodeId> requested;
    for (const auto& driver : drivers_) {
        requested.insert(driver.source_);
        if (driver.curve_clock_) requested.insert(*driver.curve_clock_);
    }
    if (current)
        for (const auto& output : outputs)
            if (requested.contains(output.node_) && std::isfinite(output.scalar_))
                sample.scalars_.emplace(output.node_, output.scalar_);
    const auto apply = [&](editor::EditResult edit) {
        if (std::holds_alternative<editor::Snapshot>(edit)) {
            result.committed_ = std::get<editor::Snapshot>(std::move(edit));
            error_.clear();
        } else {
            error_ = std::get<graph::Diagnostic>(edit).code_;
        }
    };
    ImGui::BeginDisabled(!current || gesture_active);
    const auto panel_height =
            std::max(40.0f, std::min(260.0f, ImGui::GetContentRegionAvail().y * .45f));
    ImGui::BeginChild("transform_driver.sources", {0, panel_height}, ImGuiChildFlags_Borders);
    for (const auto& driver : drivers_) {
        ImGui::PushID(driver.property_.c_str());
        const auto live = sample.scalars_.find(driver.source_);
        ImGui::Text("%s <- %s", message(driver.property_).c_str(),
                    message(driver.source_type_).c_str());
        if (!driver.signal_.empty())
            ImGui::Text("%s: %s", message("transform_driver.binding").c_str(),
                        driver.signal_.c_str());
        if (live != sample.scalars_.end())
            ImGui::Text("%s %.6g", message("transform_driver.current").c_str(),
                        std::clamp(live->second, driver.minimum_, driver.maximum_));
        if (ImGui::SmallButton(label("transform_driver.follow").c_str()))
            result.selected_ = driver.source_;
        if (driver.curve_clock_) {
            auto value = desired_.contains(driver.property_) ? desired_.at(driver.property_)
                         : live != sample.scalars_.end()
                                 ? std::clamp(live->second, driver.minimum_, driver.maximum_)
                                 : 0;
            ImGui::SetNextItemWidth(150);
            if (ImGui::InputDouble(label("transform_driver.value").c_str(), &value, 0, 0, "%.6g"))
                desired_[driver.property_] = value;
            const auto time = sample.scalars_.find(*driver.curve_clock_);
            if (time != sample.scalars_.end())
                ImGui::Text("%s %.6g s", message("transform_driver.clock").c_str(), time->second);
            ImGui::BeginDisabled(live == sample.scalars_.end() || time == sample.scalars_.end());
            if (ImGui::SmallButton(label("transform_driver.record").c_str()))
                apply(editor::RecordTransformKey(snapshot, graph::Registry{}, selected,
                                                 driver.property_, value, sample));
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("%s", message("transform_driver.shared_curve").c_str());
        }
        ImGui::PopID();
        ImGui::Separator();
    }
    ImGui::EndChild();
    const auto complete = std::all_of(drivers_.begin(), drivers_.end(), [&](const auto& driver) {
        return sample.scalars_.contains(driver.source_);
    });
    ImGui::BeginDisabled(!complete);
    if (ImGui::Button(label("transform_driver.freeze").c_str()))
        apply(editor::FreezeTransformDrivers(snapshot, graph::Registry{}, selected, sample));
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("%s", message("transform_driver.freeze_help").c_str());
    ImGui::EndDisabled();
    if (!current || !complete) ImGui::TextWrapped("%s", message("transform_driver.wait").c_str());
    if (!error_.empty()) ImGui::TextWrapped("%s", message(error_).c_str());
    return result;
}
}  // namespace rhythm::studio
