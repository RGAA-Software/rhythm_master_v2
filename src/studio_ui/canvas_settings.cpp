#include "canvas_settings.h"

#include <imgui.h>

#include <array>

namespace rhythm::studio {
std::optional<graph::Canvas> DrawCanvasSettings(graph::Canvas canvas,
                                                const std::map<std::string, std::string>& text) {
    std::optional<graph::Canvas> selected;
    const auto dimensions = std::to_string(canvas.width_) + " x " + std::to_string(canvas.height_);
    ImGui::SetNextItemWidth(200);
    if (ImGui::BeginCombo((text.at("canvas.size") + "###canvas.size").c_str(),
                          dimensions.c_str())) {
        constexpr std::array<graph::Canvas, 4> kCanvases{
                {{640, 360}, {1280, 720}, {720, 1280}, {1024, 1024}}};
        constexpr std::array kLabels{"canvas.compact", "canvas.landscape", "canvas.portrait",
                                     "canvas.square"};
        for (std::size_t index = 0; index < kCanvases.size(); ++index) {
            const auto size = kCanvases[index];
            const auto label = text.at(kLabels[index]) + "  " + std::to_string(size.width_) +
                               " x " + std::to_string(size.height_);
            if (ImGui::Selectable(label.c_str(), canvas == size)) selected = size;
        }
        ImGui::EndCombo();
    }
    return selected;
}
}  // namespace rhythm::studio
