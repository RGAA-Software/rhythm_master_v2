#include "node_inspection.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace rhythm::studio {
void NodeInspection::Draw(const graph::Document& document, graph::NodeId selected,
                          const CanvasPreviews& previews,
                          const std::map<std::string, std::string>& text) {
    if (!visible_) return;
    const auto message = [&](const std::string& key) {
        const auto found = text.find(key);
        return found == text.end() ? key : found->second;
    };
    const auto id = std::to_string(ImGui::GetID("navigation.inspection"));
    const auto available = ImGui::GetMainViewport()->WorkSize;
    ImGui::SetNextWindowSize({std::min(520.0f, available.x), std::min(380.0f, available.y)},
                             ImGuiCond_FirstUseEver);
    if (ImGui::Begin((message("navigation.inspect") + "###node.inspection." + id).c_str(),
                     &visible_)) {
        const auto node = std::find_if(document.nodes_.begin(), document.nodes_.end(),
                                       [&](const auto& value) { return value.id_ == selected; });
        if (node == document.nodes_.end()) {
            ImGui::TextUnformatted(message("select_node").c_str());
        } else {
            ImGui::Text("%s  #%llu", message(node->type_).c_str(),
                        static_cast<unsigned long long>(selected));
            ImGui::TextWrapped("%s", message("navigation.preview_help").c_str());
            const auto trace = previews.signals_.find(selected);
            const auto texture = previews.textures_.find(selected);
            const auto ready = previews.enabled_ && previews.current_;
            auto size = ImGui::GetContentRegionAvail();
            size.x = std::max(1.0f, size.x);
            size.y = std::max(1.0f, size.y);
            if (ready && trace != previews.signals_.end() && trace->second.count_) {
                const auto& value = trace->second;
                const auto samples = std::span(value.samples_).first(value.count_);
                const auto [minimum, maximum] = std::minmax_element(samples.begin(), samples.end());
                const auto padding =
                        std::max(.01f, std::max(std::abs(*minimum), std::abs(*maximum)) * .05f);
                char overlay[100]{};
                if (value.event_observation_) {
                    std::snprintf(overlay, sizeof(overlay), "%.0f | #%llu @ %.3fs", value.value_,
                                  static_cast<unsigned long long>(value.event_observation_->count_),
                                  value.event_observation_->last_seconds_);
                    ImGui::PlotHistogram("##events", samples.data(),
                                         static_cast<int>(samples.size()),
                                         static_cast<int>(value.offset_), overlay, 0,
                                         std::max(1.0f, *maximum + padding), size);
                } else {
                    std::snprintf(overlay, sizeof(overlay), "%.6g", value.value_);
                    ImGui::PlotLines("##signal", samples.data(), static_cast<int>(samples.size()),
                                     static_cast<int>(value.offset_), overlay, *minimum - padding,
                                     *maximum + padding, size);
                }
            } else if (ready && texture != previews.textures_.end()) {
                const auto width = std::min(size.x, size.y * (16.0f / 9));
                ImGui::Image(texture->second, {width, width * (9.0f / 16)});
            } else {
                ImGui::TextWrapped("%s", message(!previews.enabled_ ? "navigation.preview_disabled"
                                                                    : "navigation.preview_wait")
                                                 .c_str());
            }
        }
    }
    ImGui::End();
}
}  // namespace rhythm::studio
