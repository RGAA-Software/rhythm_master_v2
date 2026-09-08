#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>

#include "component_workbench.h"

namespace {
struct ContextDelete {
    void operator()(ImGuiContext* context) const { ImGui::DestroyContext(context); }
};
void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
// Borrow native UI windows only inside synchronous test calls. Retain value
// coordinates and IDs, never a window address across frames.
void Activate(std::string_view child, const std::string& item) {
    for (auto* window : ImGui::GetCurrentContext()->Windows)
        if (window->Active &&
            std::string_view(window->Name).find(child) != std::string_view::npos) {
            ImGui::ActivateItemByID(window->GetID(item.c_str()));
            return;
        }
    throw std::runtime_error("component timing window missing");
}
}  // namespace
int main(int argc, char* argv[]) {
    using namespace rhythm;
    try {
        Check(argc == 2, "component_timing_ui locale");
        std::ifstream input(argv[1]);
        const auto text = nlohmann::json::parse(input).get<std::map<std::string, std::string>>();
        std::unique_ptr<ImGuiContext, ContextDelete> context(ImGui::CreateContext());
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = {1400, 1100};
        io.DeltaTime = 1.0F / 60;
        Check(io.Fonts->Build(), "font atlas");
        graph::Registry registry;
        graph::ComponentDefinition definition;
        definition.type_ = "component.test.timing";
        definition.title_ = "Local timing";
        definition.nodes_ = {registry.MakeNode(1, "texture.gradient"),
                             registry.MakeNode(2, "core.time"), registry.MakeNode(3, "time.local"),
                             registry.MakeNode(4, "time.envelope"),
                             registry.MakeNode(5, "texture.affine")};
        definition.nodes_[2].properties_["speed"] = 0.5;
        definition.nodes_[3].properties_["clip_start"] = 2.0;
        definition.edges_ = {
                {1, 2, 3, "time"}, {2, 3, 4, "time"}, {3, 1, 5, "source"}, {4, 4, 5, "opacity"}};
        definition.output_ = 5;
        editor::Snapshot project;
        project.document_.id_ = "component-timing";
        project.document_.components_ = {definition};
        project.document_.nodes_ = {{10, definition.type_},
                                    registry.MakeNode(20, "output.texture")};
        project.document_.edges_ = {{1, 10, 20, "source"}};
        project.document_.output_ = 20;
        const auto initial = project;
        studio::ComponentWorkbench workbench;
        workbench.Open(project, definition.type_, 10);
        std::optional<editor::Snapshot> applied;
        ImVec2 origin{};
        float width = 0;
        const auto frame = [&] {
            ImGui::NewFrame();
            ImGui::SetNextWindowPos({0, 0});
            ImGui::SetNextWindowSize({1350, 1050});
            if (auto result = workbench.Draw(project, registry, text, "en-US"))
                applied = std::move(result);
            for (auto* window : ImGui::GetCurrentContext()->Windows)
                if (window->Active && std::string_view(window->Name).find("timeline.sections") !=
                                              std::string_view::npos) {
                    origin = window->DC.CursorStartPos;
                    width = window->WorkRect.GetWidth();
                }
            ImGui::Render();
        };
        const auto section = [&]() -> const graph::Node& {
            Check(workbench.PreviewDocument().has_value(), "component live document");
            const auto& nodes = workbench.PreviewDocument()->components_.front().nodes_;
            const auto found = std::find_if(nodes.begin(), nodes.end(),
                                            [](const auto& node) { return node.id_ == 4; });
            Check(found != nodes.end(), "component section retained");
            return *found;
        };
        frame();
        frame();
        Activate("component.inspector", "###component.timing");
        frame();
        frame();
        Check(width > 100, "component timing panel has visible bars");
        io.AddMousePosEvent(origin.x + width * 4 / 16, origin.y + 16);
        frame();
        io.AddMouseButtonEvent(0, true);
        frame();
        io.AddMousePosEvent(origin.x + width * 5 / 16, origin.y + 16);
        frame();
        frame();
        const auto tolerance = 16.0 / width + 1e-6;
        Check(std::abs(graph::Scalar(section(), "clip_start", -1) - 3) < tolerance &&
                      project == initial && !applied,
              "component section drag updates live draft without applying project history");
        io.AddMouseButtonEvent(0, false);
        frame();
        frame();
        Activate("###component.workbench", text.at("undo"));
        frame();
        frame();
        Check(graph::Scalar(section(), "clip_start", -1) == 2,
              "one component undo restores entire timing gesture");
        Activate("###component.workbench", text.at("redo"));
        frame();
        frame();
        Check(std::abs(graph::Scalar(section(), "clip_start", -1) - 3) < tolerance,
              "component timing redo");
        const auto before_add = workbench.PreviewDocument()->components_.front().nodes_.size();
        Activate("component.inspector", "###timeline.add_section");
        frame();
        frame();
        Check(workbench.PreviewDocument()->components_.front().nodes_.size() == before_add + 1 &&
                      workbench.PreviewDocument()->nodes_ == initial.document_.nodes_,
              "add reserves component-local ID and reuses its clock, never root nodes");
        Activate("###component.workbench", text.at("undo"));
        frame();
        frame();
        Check(workbench.PreviewDocument()->components_.front().nodes_.size() == before_add,
              "one undo removes component section addition");
        Activate("###component.workbench", text.at("component.apply"));
        frame();
        frame();
        Check(applied && applied->document_.nodes_ == initial.document_.nodes_ &&
                      applied->document_.components_.front().edges_ == definition.edges_ &&
                      std::abs(graph::Scalar(applied->document_.components_.front().nodes_[3],
                                             "clip_start", -1) -
                               3) < tolerance,
              "apply retains explicit local time wiring and changed internal section");
        editor::History history(initial);
        Check(history.Apply(*applied, initial.document_.revision_) && history.Undo(),
              "component apply is one root undo transaction");
        auto restored = history.Current();
        Check(restored.document_.revision_ > initial.document_.revision_,
              "undo publishes a fresh revision");
        restored.document_.revision_ = initial.document_.revision_;
        Check(restored == initial, "root undo restores complete component content and layout");
        applied.reset();
        workbench.Open(project, definition.type_, 10);
        frame();
        frame();
        Activate("component.inspector", "###timeline.add_section");
        frame();
        frame();
        Activate("###component.workbench", text.at("component.cancel"));
        frame();
        frame();
        Check(!applied && project == initial && !workbench.PreviewDocument(),
              "cancel removes timing preview without touching project");
        std::cout << "component timing: local clock wiring, live drag, undo/redo, scoped add, "
                     "apply and cancel passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
