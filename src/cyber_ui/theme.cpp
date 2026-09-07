#include "rhythm/cyber/theme.h"

#include <imgui.h>
namespace rhythm::cyber {
void ApplyTheme() {
    auto& style = ImGui::GetStyle();
    ImGui::StyleColorsDark();
    style.WindowPadding = {16, 16};
    style.FramePadding = {10, 6};
    style.ItemSpacing = {10, 9};
    style.WindowRounding = 8;
    style.FrameRounding = 5;
    style.GrabRounding = 4;
    style.Colors[ImGuiCol_WindowBg] = {0.055f, 0.07f, 0.095f, 1};
    style.Colors[ImGuiCol_TitleBgActive] = {0.08f, 0.15f, 0.19f, 1};
    style.Colors[ImGuiCol_Button] = {0.08f, 0.25f, 0.30f, 1};
    style.Colors[ImGuiCol_ButtonHovered] = {0.08f, 0.40f, 0.46f, 1};
    style.Colors[ImGuiCol_CheckMark] = {0.25f, 0.85f, 0.87f, 1};
    style.Colors[ImGuiCol_SliderGrab] = {0.25f, 0.85f, 0.87f, 1};
    style.Colors[ImGuiCol_Header] = {0.1f, 0.3f, 0.36f, 1};
}
}  // namespace rhythm::cyber
