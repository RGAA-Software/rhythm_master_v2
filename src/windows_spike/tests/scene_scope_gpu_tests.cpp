#include <bgfx/bgfx.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

#include "rhythm/project/package.h"
#include "rhythm/project/store.h"
#include "rhythm/studio/studio.h"
#include "workflow_evidence.h"

namespace {
void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
void Activate(const char* window_name, const char* label) {
    const auto* window = ImGui::FindWindowByName(window_name);
    Check(window != nullptr, "canvas Studio window missing");
    ImGui::ActivateItemByID(ImHashStr(label, 0, window->ID));
}
void Place(const char* name, ImVec2 position, ImVec2 size) {
    if (auto* window = ImGui::FindWindowByName(name)) {
        ImGui::SetWindowDock(window, 0, ImGuiCond_Always);
        ImGui::SetWindowPos(window, position, ImGuiCond_Always);
        ImGui::SetWindowSize(window, size, ImGuiCond_Always);
    }
}
// Inspect the actual image item's draw rectangle, so toolbar, DPI and aspect
// fitting participate in the mouse/pixel test instead of hard-coded centers.
ImRect OutputRect() {
    const auto* window = ImGui::FindWindowByName("###output");
    Check(window != nullptr, "output window missing");
    const auto& draw = *window->DrawList;
    for (const auto& command : draw.CmdBuffer) {
        if (command.GetTexID() == ImGui::GetIO().Fonts->TexID || command.ElemCount < 6) continue;
        const auto first =
                draw.VtxBuffer[draw.IdxBuffer[command.IdxOffset] + command.VtxOffset].pos;
        ImRect result(first, first);
        for (unsigned int index = 0; index < 6; ++index)
            result.Add(draw.VtxBuffer[draw.IdxBuffer[command.IdxOffset + index] + command.VtxOffset]
                               .pos);
        return result;
    }
    throw std::runtime_error("output image missing");
}
}  // namespace
int main(int argc, char* argv[]) {
    using namespace rhythm;
    try {
        Check(argc == 3, "resources output");
        const std::filesystem::path resources(argv[1]), root(argv[2]);
        const auto path = root / "Projects/canvas.rhythmproj";
        const auto package = root / "Published/canvas.rhythmpack";
        graph::Registry registry;
        editor::Snapshot initial;
        initial.title_ = "Component instance scope acceptance";
        auto& doc = initial.document_;
        doc.id_ = "scene-scope-acceptance";
        doc.canvas_ = {320, 180};
        graph::ComponentDefinition object;
        object.type_ = "component.test.object";
        object.nodes_ = {
                registry.MakeNode(1, "geometry.cube"), registry.MakeNode(2, "scene.transform"),
                registry.MakeNode(4, "scene.instance"), registry.MakeNode(7, "material.unlit")};
        object.nodes_[1].properties_["scale"] = .5;
        object.nodes_[1].properties_["translate_x"] = -.4;
        object.nodes_[3].properties_["color_a"] = graph::Color{1, 0, 0, 1};
        object.edges_ = {{1, 1, 4, "geometry"}, {2, 7, 4, "material"}, {3, 4, 2, "scene"}};
        object.output_ = 2;
        doc.components_ = {object};
        doc.nodes_ = {{10, object.type_},
                      {20, object.type_},
                      registry.MakeNode(30, "scene.transform"),
                      registry.MakeNode(40, "scene.merge"),
                      registry.MakeNode(5, "scene.camera"),
                      registry.MakeNode(6, "scene.render"),
                      registry.MakeNode(3, "output.texture")};
        doc.nodes_[2].properties_["translate_x"] = .8;
        doc.nodes_[4].properties_["projection"] = 1.0;
        doc.nodes_[4].properties_["orthographic_height"] = 1.0;
        doc.output_ = 3;
        doc.edges_ = {{1, 10, 40, "a"},    {2, 20, 30, "scene"}, {3, 30, 40, "b"},
                      {4, 40, 6, "scene"}, {5, 5, 6, "camera"},  {6, 6, 3, "source"}};
        initial.positions_ = {{10, {0, 0}},    {20, {0, 200}}, {30, {300, 200}}, {40, {600, 0}},
                              {5, {600, 300}}, {6, {900, 0}},  {3, {1200, 0}}};
        project::Save(path, initial);
        std::ifstream catalog_file(resources / "locales/zh-CN/studio.json");
        const auto catalog =
                nlohmann::json::parse(catalog_file).get<std::map<std::string, std::string>>();
        platform::Host host(true);
        host.Resize({1600, 1000});
        ImGui::GetIO().IniFilename = nullptr;
        auto renderer = host.CreateRenderer();
        auto font = host.CreateFontTexture(renderer);
        studio::Studio studio(resources, path);
        testing::WorkflowEvidence evidence(root);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(90);
        int frames = 0;
        const auto frame = [&] {
            Check(host.Poll() && std::chrono::steady_clock::now() < deadline,
                  "scene scope timeout");
            host.BeginUi();
            renderer.BeginFrame();
            Place("###graph", {0, 0}, {820, 990});
            Place("###inspector", {835, 0}, {750, 395});
            Place("###output", {835, 405}, {750, 585});
            Place("###component.workbench", {20, 20}, {1540, 940});
            studio.Frame(host, renderer, frames++ / 60.0);
            renderer.Submit({}, host.EndUi(), 0x111822ff);
            renderer.EndFrame();
            evidence.Record("scene-scope", studio);
        };
        const auto settle = [&] {
            for (int index = 0; index < 4; ++index) frame();
        };
        const auto ready = [&] {
            settle();
            for (int index = 0; index < 180 && !studio.HasValidPlan(); ++index) frame();
            Check(studio.HasValidPlan() && !studio.Status().budget_limited_,
                  "current scene not ready");
        };
        const auto capture = [&](const std::string& name) {
            ready();
            const auto rect = OutputRect();
            std::ofstream(root / (name + ".json")) << nlohmann::json{{"x", rect.Min.x},
                                                                     {"y", rect.Min.y},
                                                                     {"width", rect.GetWidth()},
                                                                     {"height", rect.GetHeight()}};
            bgfx::requestScreenShot(BGFX_INVALID_HANDLE, (root / name).string().c_str());
            settle();
        };
        ready();
        Activate("###output", "###canvas.edit");
        settle();
        capture("before");
        const auto image = OutputRect();
        ImGui::GetIO().AddMousePosEvent(image.Min.x + image.GetWidth() * .275f,
                                        image.Min.y + image.GetHeight() * .6f);
        frame();
        ImGui::GetIO().AddMouseButtonEvent(0, true);
        frame();
        ImGui::GetIO().AddMouseButtonEvent(0, false);
        settle();
        Check(studio.Workflow().selected_author_node_ == 10, "picked wrong component instance");
        Activate("###output", "###scene_scope.unique");
        ready();
        // Use the real nested inspector's numeric input, not a graph mutation API.
        ImGuiID property_id = 0;
        for (auto* window : ImGui::GetCurrentContext()->Windows) {
            if (window->Active && std::string_view(window->Name).find("component.inspector") !=
                                          std::string_view::npos) {
                property_id = window->GetID("###property.2.translate_x");
                ImGui::FocusWindow(window);
                ImGui::ActivateItemByID(property_id);
                ImGui::GetCurrentContext()->NavNextActivateFlags = ImGuiActivateFlags_PreferInput;
                break;
            }
        }
        Check(property_id != 0, "unique action did not open component inspector");
        frame();
        Check(ImGui::GetActiveID() == property_id, "picked author numeric field not active");
        ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl, true);
        ImGui::GetIO().AddKeyEvent(ImGuiKey_A, true);
        frame();
        ImGui::GetIO().AddKeyEvent(ImGuiKey_A, false);
        ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl, false);
        frame();
        ImGui::GetIO().AddInputCharactersUTF8("-0.65");
        frame();
        ImGui::GetIO().AddKeyEvent(ImGuiKey_Enter, true);
        frame();
        ImGui::GetIO().AddKeyEvent(ImGuiKey_Enter, false);
        settle();
        Activate("###component.workbench", catalog.at("component.apply").c_str());
        capture("moved");
        Activate("###graph", "###save");
        settle();
        Activate("###graph", "###publish");
        for (int index = 0; index < 180 && !std::filesystem::exists(package); ++index) frame();
        Check(std::filesystem::exists(package), "unique component publication missing");
        const auto saved = project::Load(path).snapshot_;
        Check(saved.document_.nodes_[0].type_ != object.type_ &&
                      saved.document_.nodes_[1].type_ == object.type_ &&
                      saved.document_.components_.size() == 2,
              "unique action changed the wrong instance or lost definition");
        for (const auto& definition : saved.document_.components_) {
            const auto transform = std::find_if(definition.nodes_.begin(), definition.nodes_.end(),
                                                [](const auto& node) { return node.id_ == 2; });
            Check(transform != definition.nodes_.end(), "authored transform missing");
            const auto expected = definition.type_ == object.type_ ? -.4 : -.65;
            Check(std::abs(graph::Scalar(*transform, "translate_x", 0) - expected) < 1e-8,
                  "shared definition changed or unique edit was not saved");
        }
        Check(project::EncodeProgram(project::LoadPackage(package).program_) ==
                      project::EncodeProgram(std::get<graph::ExecutionPlan>(
                              graph::Compile(saved.document_, registry))),
              "publication differs from saved unique component");
        Activate("###graph", "###undo");
        capture("undone");
        Activate("###graph", "###reopen");
        const auto old_generation = studio.Workflow().installed_generation_;
        for (int index = 0;
             index < 180 && studio.Workflow().installed_generation_ == old_generation; ++index)
            frame();
        Check(studio.Workflow().installed_generation_ != old_generation,
              "reopen retained old plan");
        capture("reopened");
        std::cout
                << "Actual component pick, unique scope, inspector edit, save/publish/undo/reopen: "
                << root << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
