#include "preview_routing.h"

#include <imgui.h>

#include <algorithm>
#include <set>
#include <utility>

#include "rhythm/runtime/viewers.h"

namespace rhythm::studio {
PreviewRequest PreviewRouting::Prepare(std::vector<graph::NodeId> roots,
                                       editor::ScopedViewers scoped, std::string document_id) {
    const auto unique = [](auto& nodes) {
        std::set<graph::NodeId> seen;
        std::erase_if(nodes, [&](auto id) { return !seen.insert(id).second; });
    };
    unique(roots);
    unique(scoped.nodes_);
    if (document_id != document_id_ || roots != demand_.roots_ ||
        scoped.nodes_ != demand_.scoped_.nodes_ ||
        scoped.instance_path_ != demand_.scoped_.instance_path_)
        page_ = 0;
    document_id_ = std::move(document_id);
    demand_ = {std::move(roots), std::move(scoped)};
    constexpr auto kLimit = runtime::Viewers::kMaxPreviews;
    pages_ = (demand_.roots_.size() + demand_.scoped_.nodes_.size() + kLimit - 1) / kLimit;
    PreviewRequest result;
    result.scoped_.instance_path_ = demand_.scoped_.instance_path_;
    std::size_t slot = 0;
    for (std::size_t index = 0;
         index < std::max(demand_.roots_.size(), demand_.scoped_.nodes_.size()); ++index) {
        if (index < demand_.roots_.size()) {
            if (slot / kLimit == page_) result.roots_.push_back(demand_.roots_[index]);
            ++slot;
        }
        if (index < demand_.scoped_.nodes_.size()) {
            if (slot / kLimit == page_)
                result.scoped_.nodes_.push_back(demand_.scoped_.nodes_[index]);
            ++slot;
        }
    }
    scoped_nodes_.clear();
    invalidated_ = true;
    return result;
}
bool PreviewRouting::StepPage(int direction) {
    if (pages_ < 2 || direction == 0) return false;
    page_ = direction > 0 ? (page_ + 1) % pages_ : (page_ + pages_ - 1) % pages_;
    page_changed_ = true;
    return true;
}
bool PreviewRouting::TakePageChange() { return std::exchange(page_changed_, false); }
void PreviewRouting::DrawNavigation(const std::map<std::string, std::string>& text) {
    if (!pages_) return;
    ImGui::PushID("preview.navigation");
    ImGui::BeginDisabled(pages_ < 2);
    if (ImGui::SmallButton((text.at("preview.previous") + "###previous").c_str())) StepPage(-1);
    ImGui::SameLine();
    ImGui::Text("%s %zu / %zu", text.at("preview.group").c_str(), page_ + 1, pages_);
    ImGui::SameLine();
    if (ImGui::SmallButton((text.at("preview.next") + "###next").c_str())) StepPage(1);
    ImGui::EndDisabled();
    if (pages_ > 1 && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("%s", text.at("preview.group_help").c_str());
    ImGui::PopID();
}
void PreviewRouting::Stage(const editor::Compilation& compilation) {
    pending_nodes_ = compilation.viewers_;
    pending_scoped_nodes_ = compilation.scoped_nodes_;
    pending_signal_nodes_.clear();
    if (const auto plan = std::get_if<graph::ExecutionPlan>(&compilation.result_)) {
        const graph::Registry registry;
        for (const auto& instruction : plan->instructions_) {
            if (std::find(pending_nodes_.begin(), pending_nodes_.end(), instruction.node_.id_) ==
                pending_nodes_.end())
                continue;
            const auto descriptor = registry.Find(instruction.node_.type_);
            if (descriptor && (descriptor->output_ == graph::ValueType::kScalar ||
                               descriptor->output_ == graph::ValueType::kSignal))
                pending_signal_nodes_.push_back(instruction.node_.id_);
        }
    }
}
void PreviewRouting::Commit() {
    active_nodes_ = std::move(pending_nodes_);
    signal_nodes_ = std::move(pending_signal_nodes_);
    scoped_nodes_ = std::move(pending_scoped_nodes_);
    invalidated_ = true;
}
bool PreviewRouting::TakeInvalidation() { return std::exchange(invalidated_, false); }
CanvasPreviews PreviewRouting::Scoped(const CanvasPreviews& previews) const {
    CanvasPreviews result;
    result.enabled_ = previews.enabled_;
    for (const auto& [local, expanded] : scoped_nodes_) {
        if (const auto texture = previews.textures_.find(expanded);
            texture != previews.textures_.end())
            result.textures_[local] = texture->second;
        if (const auto signal = previews.signals_.find(expanded); signal != previews.signals_.end())
            result.signals_[local] = signal->second;
    }
    return result;
}
}  // namespace rhythm::studio
