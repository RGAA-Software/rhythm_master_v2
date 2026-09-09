#include <imgui.h>
#include <imgui_internal.h>

#include <array>
#include <cstddef>

// Compiled only against the installed SDK headers, without linking its ImGui.
std::array<std::size_t, 10> InstalledImGuiLayout() {
    return {sizeof(ImGuiIO),
            sizeof(ImGuiStyle),
            sizeof(ImGuiContext),
            sizeof(ImGuiWindow),
            sizeof(ImDrawList),
            offsetof(ImGuiIO, MousePos),
            offsetof(ImGuiIO, MouseDown),
            offsetof(ImGuiContext, HoveredWindow),
            offsetof(ImGuiContext, ActiveId),
            offsetof(ImGuiWindow, InnerRect)};
}
