#include <imgui.h>

#include <algorithm>
#include <array>

#include "property_inspector.h"
#include "rhythm/graph/controls.h"

namespace rhythm::studio {
void PropertyInspector::PerformControls(parameters::ControlValues values) {
    performance_controls_ = std::move(values);
    live_controls_.clear();
}
parameters::ControlValues PropertyInspector::LiveControls(
        const parameters::ControlBank& bank) const {
    parameters::ControlValues result;
    for (const auto& control : bank.Definitions())
        if (const auto found = performance_controls_.find(control.id_);
            found != performance_controls_.end() && found->second >= control.minimum_ &&
            found->second <= control.maximum_)
            result.emplace(*found);
    for (const auto& control : bank.Definitions())
        if (const auto found = live_controls_.find(control.id_);
            found != live_controls_.end() && found->second >= control.minimum_ &&
            found->second <= control.maximum_)
            result[found->first] = found->second;
    return result;
}
bool PropertyInspector::DrawControls(const editor::Snapshot& snapshot,
                                     const std::map<std::string, std::string>& text,
                                     InspectorResult& result, double seconds, bool defer_recall) {
    try {
        const auto bank = graph::DescribeControls(snapshot.document_);
        std::erase_if(live_controls_, [&](const auto& item) {
            return std::none_of(
                    bank.Definitions().begin(), bank.Definitions().end(), [&](const auto& control) {
                        return control.id_ == item.first && control.value_ == item.second;
                    });
        });
        if (sequence_bank_ != bank || sequence_cues_ != snapshot.document_.control_cues_) {
            std::optional<parameters::ControlSequence> sequence;
            if (!snapshot.document_.control_cues_.empty())
                sequence.emplace(bank, snapshot.document_.control_cues_);
            control_sequence_ = std::move(sequence);
            sequence_bank_ = bank;
            sequence_cues_ = snapshot.document_.control_cues_;
        }
        const auto current =
                parameters::EvaluateControls(bank, control_sequence_, seconds, LiveControls(bank));
        auto edit = controls_.Draw(bank, current, text, true, defer_recall);
        if (defer_recall) result.recall_ = edit.recall_;
        if (control_sequence_) {
            const auto found = text.find("cue.follow");
            const auto title =
                    (found == text.end() ? "cue.follow" : found->second) + "###cue.follow";
            if (ImGui::Button(title.c_str())) {
                result.follow_cues_ = true;
                live_controls_.clear();
                performance_controls_.clear();
            }
        }
        if (edit.values_ || !edit.capture_.empty() || edit.remove_) {
            auto next = snapshot;
            if (edit.values_)
                for (const auto& [id, value] : *edit.values_) {
                    performance_controls_.erase(id);
                    live_controls_[id] = value;
                }
            if (edit.values_)
                for (auto& node : next.document_.nodes_)
                    if (node.type_ == "control.scalar" && edit.values_->contains(node.id_))
                        node.properties_["value"] = edit.values_->at(node.id_);
            if (edit.remove_)
                std::erase_if(next.document_.control_snapshots_,
                              [&](const auto& item) { return item.id_ == edit.remove_; });
            graph::PruneControls(next.document_);
            if (!edit.capture_.empty()) {
                std::uint64_t id = 1;
                while (std::any_of(next.document_.control_snapshots_.begin(),
                                   next.document_.control_snapshots_.end(),
                                   [&](const auto& item) { return item.id_ == id; }))
                    ++id;
                next.document_.control_snapshots_.push_back({id, edit.capture_, current});
            }
            (void)graph::DescribeControls(next.document_);
            draft_ = std::move(next);
            result.preview_changed_ = true;
        }
        if ((edit.committed_ || !edit.capture_.empty() || edit.remove_) && draft_) {
            result.committed_ = std::move(draft_);
            draft_.reset();
            return true;
        }
        if (result.preview_changed_) return true;
        const auto names = text.find("controls.rename");
        const auto heading =
                (names == text.end() ? "controls.rename" : names->second) + "###control_names";
        if (!bank.Definitions().empty() && ImGui::TreeNode(heading.c_str())) {
            for (const auto& control : bank.Definitions()) {
                std::array<char, 129> title{};
                std::copy(control.title_.begin(), control.title_.end(), title.begin());
                const auto label = "###control_name." + std::to_string(control.id_);
                ImGui::Text("#%llu", static_cast<unsigned long long>(control.id_));
                ImGui::SameLine();
                if (ImGui::InputText(label.c_str(), title.data(), title.size(),
                                     ImGuiInputTextFlags_EnterReturnsTrue) &&
                    title[0]) {
                    auto next = snapshot;
                    next.document_.control_titles_[control.id_] = title.data();
                    (void)graph::DescribeControls(next.document_);
                    result.committed_ = std::move(next);
                    draft_.reset();
                    ImGui::TreePop();
                    return true;
                }
            }
            ImGui::TreePop();
        }
    } catch (const std::exception&) {
        result.diagnostic_ = graph::Diagnostic{"graph.controls"};
    }
    return false;
}
}  // namespace rhythm::studio
