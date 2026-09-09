#include <imgui.h>
#include <imgui_internal.h>

#include <array>
#include <cstddef>
#include <iostream>

std::array<std::size_t, 10> InstalledImGuiLayout();
int main() {
    const std::array<std::size_t, 10> current{sizeof(ImGuiIO),
                                              sizeof(ImGuiStyle),
                                              sizeof(ImGuiContext),
                                              sizeof(ImGuiWindow),
                                              sizeof(ImDrawList),
                                              offsetof(ImGuiIO, MousePos),
                                              offsetof(ImGuiIO, MouseDown),
                                              offsetof(ImGuiContext, HoveredWindow),
                                              offsetof(ImGuiContext, ActiveId),
                                              offsetof(ImGuiWindow, InnerRect)};
    const auto installed = InstalledImGuiLayout();
    std::cout << "project ImGui " << IMGUI_VERSION << "\ninstalled/project: ";
    for (std::size_t index = 0; index < current.size(); ++index)
        std::cout << installed[index] << '/' << current[index] << ' ';
    std::cout << "\ninstalled ImGuizmo binary layout compatible: " << (installed == current)
              << '\n';
    // The probe records a decision; a mismatch is not license to execute a
    // binary that accesses the wrong ImGuiContext/IO layout.
}
