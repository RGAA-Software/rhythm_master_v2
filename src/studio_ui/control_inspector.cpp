#include <imgui.h>

#include <algorithm>
#include <array>

#include "property_inspector.h"
#include "rhythm/graph/controls.h"

namespace rhythm::studio {
bool PropertyInspector::DrawControls(const editor::Snapshot& snapshot,
                                     const std::map<std::string, std::string>& text,
                                     InspectorResult& result) {
    try {
        const auto bank = graph::DescribeControls(snapshot.document_);
        auto edit = controls_.Draw(bank, {}, text, true);
        if (edit.values_ || !edit.capture_.empty() || edit.remove_) {
            auto next = snapshot;
            if (edit.values_)
                for (auto& node : next.document_.nodes_)
                    if (node.type_ == "control.scalar")
                        node.properties_["value"] = edit.values_->at(node.id_);
            if (edit.remove_)
                std::erase_if(next.document_.control_snapshots_,
                              [&](const auto& item) { return item.id_ == edit.remove_; });
            if (!edit.capture_.empty()) {
                std::uint64_t id = 1;
                while (std::any_of(next.document_.control_snapshots_.begin(),
                                   next.document_.control_snapshots_.end(),
                                   [&](const auto& item) { return item.id_ == id; }))
                    ++id;
                next.document_.control_snapshots_.push_back(
                        {id, edit.capture_, graph::DescribeControls(next.document_).Resolve()});
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
