#include <imgui.h>
#include <imgui_internal.h>

#include <iostream>
#include <memory>
#include <stdexcept>

#include "component_workbench.h"

namespace {
struct ContextDeleter {
    void operator()(ImGuiContext* context) const { ImGui::DestroyContext(context); }
};
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void Run() {
    using namespace rhythm;
    std::unique_ptr<ImGuiContext, ContextDeleter> context(ImGui::CreateContext());
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = {1400, 900};
    io.DeltaTime = 1.0f / 60;
    Require(io.Fonts->Build(), "font atlas");
    graph::Registry registry;
    graph::ComponentDefinition definition;
    definition.type_ = "component.test.card";
    definition.nodes_ = {registry.MakeNode(1, "texture.shape")};
    definition.output_ = 1;
    definition.parameters_ = {{"width", 1, "shape_width", "Shape"}};
    editor::Snapshot project;
    project.document_.id_ = "component-window";
    project.document_.components_ = {definition};
    project.document_.nodes_ = {{10, definition.type_}, registry.MakeNode(20, "output.texture")};
    project.document_.edges_ = {{1, 10, 20, "source"}};
    project.document_.output_ = 20;
    project.positions_ = {{10, {20, 20}}, {20, {330, 20}}};
    studio::GraphCanvas root;
    studio::ComponentWorkbench workbench;
    workbench.Open(project, definition.type_);
    std::optional<editor::Snapshot> applied;
    const auto frame = [&] {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize({1300, 850});
        ImGui::Begin("Root");
        root.Draw(project, registry, {});
        ImGui::End();
        ImGui::SetNextWindowPos({30, 30});
        if (auto result = workbench.Draw(project, registry, {}, "en-US"))
            applied = std::move(result);
        ImGui::Render();
    };
    for (int index = 0; index < 8; ++index) frame();
    Require(!applied, "opening draft does not mutate project");
    // Borrow ImGui's window only during this synchronous adapter query; clicks
    // are injected into ImGui, never the user's OS mouse.
    const auto apply_position = [] {
        const auto* window = ImGui::FindWindowByName("component.edit###component.workbench");
        if (!window) throw std::runtime_error("workbench window");
        return ImVec2{window->DC.CursorStartPos.x + 20,
                      window->DC.CursorStartPos.y + ImGui::GetFontSize() +
                              ImGui::GetStyle().ItemSpacing.y + 8};
    }();
    io.AddMousePosEvent(apply_position.x, apply_position.y);
    frame();
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    frame();
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    frame();
    Require(applied && applied->document_ == project.document_,
            "apply returns the validated project transaction");
    applied.reset();
    frame();
    Require(!applied, "applied draft closes");
    std::cout << "component UI: simultaneous canvases, draft isolation, apply and close passed\n";
}
}  // namespace
int main() {
    try {
        Run();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
