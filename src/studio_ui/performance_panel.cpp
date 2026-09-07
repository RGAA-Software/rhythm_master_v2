#include "performance_panel.h"

#include <imgui.h>

#include <algorithm>
#include <numeric>

namespace rhythm::studio {
bool DrawPerformancePanel(const runtime::FrameResult& frame, const render::FrameStats& stats,
                          graph::NodeId selected, bool& reuse_textures,
                          const std::map<std::string, std::string>& text) {
    if (!ImGui::CollapsingHeader((text.at("profile.title") + "###profile").c_str())) return false;
    ImGui::Checkbox((text.at("profile.reuse") + "###profile.reuse").c_str(), &reuse_textures);
    const auto cpu_ms =
            std::accumulate(frame.profiles_.begin(), frame.profiles_.end(), 0.0,
                            [](double sum, const auto& node) { return sum + node.cpu_ms_; });
    ImGui::Text("%s: %.3f ms", text.at("profile.cpu_total").c_str(), cpu_ms);
    ImGui::Text("%s: %.1f MiB / %u", text.at("profile.textures").c_str(),
                static_cast<double>(stats.texture_bytes_) / (1024 * 1024), stats.live_textures_);
    ImGui::Text("%s: %u", text.at("profile.recycled").c_str(), frame.recycled_textures_);
    ImGui::TextWrapped("%s", text.at("profile.help").c_str());
    if (const auto found = std::find_if(frame.profiles_.begin(), frame.profiles_.end(),
                                        [&](const auto& item) { return item.node_ == selected; });
        found != frame.profiles_.end())
        ImGui::Text("%s %llu: %.3f ms / %u", text.at("profile.selected").c_str(),
                    static_cast<unsigned long long>(selected), found->cpu_ms_, found->passes_);
    if (ImGui::BeginTable("profile.nodes", 4,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable,
                          {0, 220})) {
        ImGui::TableSetupScrollFreeze(0, 1);
        for (const auto key : {"profile.node", "profile.cpu", "profile.passes", "profile.delta"})
            ImGui::TableSetupColumn(text.at(key).c_str());
        ImGui::TableHeadersRow();
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(frame.profiles_.size()));
        while (clipper.Step()) {
            for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index) {
                const auto& item = frame.profiles_[index];
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%llu", static_cast<unsigned long long>(item.node_));
                ImGui::TableNextColumn();
                ImGui::Text("%.3f", item.cpu_ms_);
                ImGui::TableNextColumn();
                ImGui::Text("%u", item.passes_);
                ImGui::TableNextColumn();
                ImGui::Text("%+.1f", static_cast<double>(item.texture_delta_bytes_) / 1024);
            }
        }
        ImGui::EndTable();
    }
    return true;
}
}  // namespace rhythm::studio
