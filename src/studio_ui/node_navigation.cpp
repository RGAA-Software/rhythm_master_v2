#include "node_navigation.h"

#include <algorithm>

#include "rhythm/graph/bindings.h"

namespace rhythm::studio {
void NodeNavigation::Reset() {
    dirty_ = true;
    filter_.Clear();
}
NodeNavigationAction NodeNavigation::Draw(const graph::Document& document, graph::NodeId selected,
                                          const std::map<std::string, std::string>& text) {
    const auto message = [&](const std::string& key) {
        const auto found = text.find(key);
        return found == text.end() ? key : found->second;
    };
    const auto label = [&](const std::string& key) { return message(key) + "###" + key; };
    bool row_started = false;
    const auto button = [&](const std::string& key) {
        const auto width =
                ImGui::CalcTextSize(message(key).c_str()).x + 2 * ImGui::GetStyle().FramePadding.x;
        const auto right = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
        if (row_started &&
            ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + width <= right)
            ImGui::SameLine();
        row_started = true;
        return ImGui::SmallButton(label(key).c_str());
    };
    NodeNavigationAction result;
    const auto open = [&](int direction) {
        direction_ = direction;
        filter_.Clear();
        ImGui::OpenPopup("navigation.results");
    };
    if (button("navigation.find")) open(0);
    if (button("navigation.fit")) result.fit_ = true;
    const auto valid_selection =
            std::any_of(document.nodes_.begin(), document.nodes_.end(),
                        [&](const auto& node) { return node.id_ == selected; });
    ImGui::BeginDisabled(!valid_selection);
    if (button("navigation.focus")) result.focus_ = true;
    if (button("navigation.upstream")) open(-1);
    if (button("navigation.downstream")) open(1);
    if (button("navigation.inspect")) {
        result.focus_ = true;
        result.inspect_ = true;
    }
    ImGui::EndDisabled();
    // Match the existing palette's explicit sizing, avoiding popup/child auto-
    // sizing feedback. The viewport, not a fixed desktop resolution, is the cap.
    const auto available = ImGui::GetMainViewport()->WorkSize;
    ImGui::SetNextWindowSize({std::min(470.0f, std::max(1.0f, available.x - 16)),
                              std::min(480.0f, std::max(1.0f, available.y - 16))});
    if (!ImGui::BeginPopup("navigation.results")) return result;
    const auto language = message("graph");
    if (dirty_ || document_id_ != document.id_ || revision_ != document.revision_ ||
        language_ != language) {
        entries_.clear();
        upstream_.clear();
        downstream_.clear();
        for (const auto& node : document.nodes_) {
            auto title = message(node.type_);
            if (title == node.type_) {
                const auto definition = std::find_if(
                        document.components_.begin(), document.components_.end(),
                        [&](const auto& component) { return component.type_ == node.type_; });
                if (definition != document.components_.end() && !definition->title_.empty())
                    title = definition->title_;
            }
            const auto id = std::to_string(node.id_);
            entries_.push_back({node.id_, title + "  #" + id, title + " " + node.type_ + " " + id});
        }
        const auto resolved = graph::ResolveEdges(document);
        invalid_bindings_ = !std::holds_alternative<std::vector<graph::Edge>>(resolved);
        // Invalid named bindings must not appear as a complete dependency list.
        if (!invalid_bindings_)
            for (const auto& edge : std::get<std::vector<graph::Edge>>(resolved)) {
                upstream_[edge.to_].insert(edge.from_);
                downstream_[edge.from_].insert(edge.to_);
            }
        document_id_ = document.id_;
        revision_ = document.revision_;
        language_ = language;
        dirty_ = false;
    }
    ImGui::TextUnformatted(message(direction_ < 0   ? "navigation.upstream"
                                   : direction_ > 0 ? "navigation.downstream"
                                                    : "navigation.find")
                                   .c_str());
    if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
    filter_.Draw("###navigation.search", -1);
    std::vector<std::size_t> matches;
    const auto& adjacency = direction_ < 0 ? upstream_ : downstream_;
    const auto neighbors = adjacency.find(selected);
    for (std::size_t index = 0; index < entries_.size(); ++index) {
        const auto& entry = entries_[index];
        if (direction_ && (invalid_bindings_ || neighbors == adjacency.end() ||
                           !neighbors->second.contains(entry.id_)))
            continue;
        if (filter_.PassFilter(entry.searchable_.c_str())) matches.push_back(index);
    }
    if (direction_ && invalid_bindings_)
        ImGui::TextWrapped("%s", message("navigation.invalid_bindings").c_str());
    else if (matches.empty())
        ImGui::TextDisabled("%s", message("palette.empty").c_str());
    ImGui::BeginChild("navigation.rows");
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(matches.size()));
    while (clipper.Step())
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
            const auto& entry = entries_[matches[static_cast<std::size_t>(row)]];
            const auto item = entry.label_ + "###node." + std::to_string(entry.id_);
            if (ImGui::Selectable(item.c_str(), selected == entry.id_)) {
                result.selected_ = entry.id_;
                result.focus_ = true;
                ImGui::CloseCurrentPopup();
            }
        }
    ImGui::EndChild();
    ImGui::EndPopup();
    return result;
}
}  // namespace rhythm::studio
