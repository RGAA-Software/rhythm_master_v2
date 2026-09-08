#include <imgui.h>
#include <imgui_internal.h>

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string_view>

#include "component_panel.h"
#include "component_workbench.h"
#include "preview_routing.h"

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
    project.document_.nodes_[0].properties_["width"] = 0.7;
    project.document_.edges_ = {{1, 10, 20, "source"}};
    project.document_.output_ = 20;
    project.positions_ = {{10, {20, 20}}, {20, {330, 20}}};
    studio::GraphCanvas root;
    studio::ComponentWorkbench workbench;
    workbench.Open(project, definition.type_);
    std::optional<editor::Snapshot> applied;
    studio::CanvasPreviews previews{true, {{1, 123}}};
    const auto frame = [&] {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize({1300, 850});
        ImGui::Begin("Root");
        root.Draw(project, registry, {});
        ImGui::End();
        ImGui::SetNextWindowPos({30, 30});
        rhythm::studio::PreviewRouting routing;
        if (auto result = workbench.Draw(project, registry, {}, "en-US", routing, previews))
            applied = std::move(result);
        ImGui::Render();
    };
    for (int index = 0; index < 8; ++index) frame();
    Require(!applied, "opening draft does not mutate project");
    Require(workbench.PreviewDocument() &&
                    workbench.PreviewViewers().instance_path_ == std::vector<graph::NodeId>{10} &&
                    workbench.DrawnPreviews() == 1,
            "body viewer requests the concrete root instance and displays its texture");
    const auto preview_generation = workbench.PreviewGeneration();
    frame();
    Require(workbench.PreviewGeneration() == preview_generation,
            "idle workbench does not continuously request compilation");
    const auto hide_parameter = [] {
        // Short-lived ImGui borrowing remains inside this synchronous test adapter.
        for (auto* window : ImGui::GetCurrentContext()->Windows) {
            if (window->Active && std::string_view(window->Name).find("component.inspector") !=
                                          std::string_view::npos) {
                ImGui::ActivateItemByID(ImHashStr("component.hide", 0, window->GetID(256)));
                return;
            }
        }
        throw std::runtime_error("component inspector child");
    };
    hide_parameter();
    frame();
    frame();
    Require(workbench.PreviewDocument() &&
                    !workbench.PreviewDocument()->nodes_[0].properties_.contains("width") &&
                    project.document_.nodes_[0].properties_.contains("width"),
            "interface draft updates the live document without committing project history");
    {
        auto* window = ImGui::FindWindowByName("###component.workbench");
        Require(window != nullptr, "workbench undo boundary");
        ImGui::ActivateItemByID(window->GetID("undo"));
    }
    frame();
    frame();
    Require(workbench.PreviewDocument()->nodes_[0].properties_.at("width") == graph::Property{0.7},
            "undo restores the preview's instance override");
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
    Require(!workbench.PreviewDocument() && workbench.PreviewViewers().nodes_.empty(),
            "closing draft releases its live preview demand");
    studio::PreviewRouting routing;
    const auto request = routing.Prepare({1, 2, 3, 4, 5, 6, 7, 8}, {{10}, {1, 2, 3, 4, 5, 6}});
    Require(request.roots_.size() == 4 && request.scoped_.nodes_.size() == 4,
            "component and root demand each receive four slots in the shared first group");
    editor::Compilation compilation;
    compilation.viewers_ = {101};
    compilation.scoped_nodes_ = {{2, 101}};
    routing.Stage(compilation);
    studio::CanvasPreviews rendered{true, {{101, 777}}};
    rendered.signals_[101].value_ = 0.375;
    Require(routing.Scoped(rendered).textures_.empty(),
            "completed compilation cannot map textures before its resources are committed");
    routing.Commit();
    Require(routing.Scoped(rendered).textures_.at(2) == 777,
            "committed instance map selects its own existing runtime image");
    Require(routing.Scoped(rendered).signals_.at(2).value_ == 0.375,
            "numeric previews must use the same committed instance mapping");
    routing.Prepare({}, {{20}, {2}});
    Require(routing.Scoped(rendered).textures_.empty() &&
                    routing.Scoped(rendered).signals_.empty() && routing.TakeInvalidation() &&
                    !routing.TakeInvalidation(),
            "switching instance hides the old mapping and invalidates cached images once");
    studio::ComponentPanel panel;
    std::optional<studio::ComponentAction> action;
    const auto panel_frame = [&](const char* activate = "") {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize({1100, 800});
        ImGui::Begin("Unpack panel");
        if (*activate) ImGui::ActivateItemByID(ImGui::GetID(activate));
        if (const auto requested =
                    panel.Draw(project.document_, std::vector<graph::NodeId>{10}, {}))
            action = requested;
        ImGui::End();
        ImGui::Render();
    };
    panel_frame();
    panel_frame("###component.panel");
    panel_frame();
    panel_frame("###component.unpack");
    panel_frame();
    Require(action && action->kind_ == studio::ComponentActionKind::kUnpack,
            "selected instance unpack is reachable through the component panel");
    const auto unpacked = studio::ExecuteComponentAction(*action, project, registry,
                                                         std::vector<graph::NodeId>{10}, 30, {});
    Require(std::holds_alternative<editor::Snapshot>(unpacked) &&
                    std::get<editor::Snapshot>(unpacked).document_.nodes_.back().type_ ==
                            "texture.shape",
            "unpack UI action reaches the pure authoring command");
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
