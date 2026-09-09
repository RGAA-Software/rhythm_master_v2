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
ImRect OutputRect(const char* name = "###output") {
    const auto* window = ImGui::FindWindowByName(name);
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
        Check(argc >= 3 && argc <= 5, "resources output [--scene|--automation] [--named]");
        bool scene_mode = false, automation_mode = false, named_mode = false;
        for (int index = 3; index < argc; ++index) {
            const std::string_view flag(argv[index]);
            Check(flag == "--scene" || flag == "--automation" || flag == "--named",
                  "unknown canvas mode");
            scene_mode |= flag == "--scene";
            automation_mode |= flag == "--automation";
            named_mode |= flag == "--named";
        }
        Check(!(scene_mode && automation_mode) && !(named_mode && automation_mode),
              "incompatible canvas modes");
        const std::filesystem::path resources(argv[1]), root(argv[2]);
        const auto path = root / "Projects/canvas.rhythmproj";
        const auto package = root / "Published/canvas.rhythmpack";
        graph::Registry registry;
        editor::Snapshot initial;
        initial.title_ = "Direct 2D canvas acceptance";
        auto& doc = initial.document_;
        doc.id_ = "direct-canvas-acceptance";
        doc.canvas_ = {320, 180};
        doc.nodes_ = {registry.MakeNode(1, "texture.gradient"),
                      registry.MakeNode(2, "texture.affine"),
                      registry.MakeNode(3, "output.texture")};
        doc.nodes_[0].properties_["color_a"] = graph::Color{1, 0, 0, 1};
        doc.nodes_[0].properties_["color_b"] = graph::Color{1, 0, 0, 1};
        doc.nodes_[1].properties_["scale"] = .5;
        doc.edges_ = {{1, 1, 2, "source"}, {2, 2, 3, "source"}};
        doc.output_ = 3;
        initial.positions_ = {{1, {0, 0}}, {2, {300, 0}}, {3, {600, 0}}};
        if (automation_mode) {
            doc.nodes_.push_back(registry.MakeNode(4, "scalar.constant"));
            doc.nodes_.back().properties_["value"] = 0.0;
            doc.edges_.push_back({3, 4, 2, "translate_x"});
            initial.positions_[4] = {0, 300};
        }
        if (scene_mode) {
            doc.nodes_ = {
                    registry.MakeNode(1, "geometry.cube"),  registry.MakeNode(2, "scene.transform"),
                    registry.MakeNode(3, "output.texture"), registry.MakeNode(4, "scene.instance"),
                    registry.MakeNode(5, "scene.camera"),   registry.MakeNode(6, "scene.render"),
                    registry.MakeNode(7, "material.unlit")};
            doc.nodes_[4].properties_["projection"] = 1.0;
            doc.nodes_[4].properties_["orthographic_height"] = 1.0;
            doc.nodes_[6].properties_["color_a"] = graph::Color{1, 0, 0, 1};
            doc.edges_ = {{1, 1, 4, "geometry"}, {2, 7, 4, "material"}, {3, 4, 2, "scene"},
                          {4, 2, 6, "scene"},    {5, 5, 6, "camera"},   {6, 6, 3, "source"}};
            initial.positions_ = {{1, {0, 0}},     {7, {0, 250}}, {4, {300, 0}}, {2, {600, 0}},
                                  {5, {600, 330}}, {6, {900, 0}}, {3, {1200, 0}}};
        }
        if (named_mode) {
            for (const auto& edge : doc.edges_) {
                const auto name = "source-" + std::to_string(edge.from_);
                if (!std::any_of(doc.signals_.begin(), doc.signals_.end(),
                                 [&](const auto& signal) { return signal.name_ == name; }))
                    doc.signals_.push_back({name, edge.from_});
                doc.bindings_.push_back({edge.to_, edge.input_, name});
            }
            doc.edges_.clear();
        }
        project::Save(path, initial);
        const auto initial_saved = project::Load(path).snapshot_.document_;
        platform::Host host(true);
        host.Resize({1600, 1000});
        ImGui::GetIO().IniFilename = nullptr;
        auto renderer = host.CreateRenderer();
        auto font = host.CreateFontTexture(renderer);
        studio::Studio studio(resources, path);
        testing::WorkflowEvidence evidence(root);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(75);
        int frames = 0;
        const auto frame = [&] {
            Check(host.Poll() && std::chrono::steady_clock::now() < deadline,
                  "canvas Studio timeout");
            host.BeginUi();
            renderer.BeginFrame();
            Place("###graph", {0, 0}, {820, 990});
            Place("###inspector", {835, 0}, {750, 395});
            Place("###output", {835, 405}, {750, 585});
            studio.Frame(host, renderer, frames++ / 60.0);
            renderer.Submit({}, host.EndUi(), 0x111822ff);
            renderer.EndFrame();
            evidence.Record("canvas", studio);
        };
        const auto settle = [&] {
            for (int index = 0; index < 4; ++index) frame();
        };
        const auto ready = [&] {
            settle();
            for (int index = 0; index < 150 && !studio.HasValidPlan(); ++index) frame();
            Check(studio.HasValidPlan() && !studio.Status().budget_limited_,
                  "current output not ready");
        };
        ready();
        const auto click = [&](float x, float y) {
            ImGui::GetIO().AddMousePosEvent(x, y);
            frame();
            ImGui::GetIO().AddMouseButtonEvent(0, true);
            frame();
            ImGui::GetIO().AddMouseButtonEvent(0, false);
            frame();
        };
        if (!scene_mode) {
            Activate("###graph", "###navigation.find");
            settle();
            ImGui::GetIO().AddInputCharactersUTF8("texture.affine");
            settle();
            bool found = false;
            for (const auto* window : ImGui::GetCurrentContext()->Windows)
                if (window->Active && std::string_view(window->Name).find("navigation.rows") !=
                                              std::string_view::npos) {
                    ImGui::ActivateItemByID(ImHashStr("###node.2", 0, window->ID));
                    found = true;
                    break;
                }
            Check(found, "actual node search results missing");
            settle();
            Check(studio.Workflow().selected_author_node_ == 2,
                  "could not find and focus affine author node");
        }
        ready();
        if (!scene_mode) {
            Activate("###graph", "###navigation.inspect");
            ready();
            std::string inspection;
            for (const auto* window : ImGui::GetCurrentContext()->Windows)
                if (window->Active && std::string_view(window->Name).find("###node.inspection.") !=
                                              std::string_view::npos)
                    inspection = window->Name;
            Check(!inspection.empty(), "selected output inspection missing");
            Place(inspection.c_str(), {100, 350}, {520, 380});
            ready();
            const auto image = OutputRect(inspection.c_str());
            std::ofstream(root / "inspection.json")
                    << nlohmann::json{{"x", image.Min.x},
                                      {"y", image.Min.y},
                                      {"width", image.GetWidth()},
                                      {"height", image.GetHeight()}};
            bgfx::requestScreenShot(BGFX_INVALID_HANDLE, (root / "inspection").string().c_str());
            settle();
            // Close through the real title-bar button, restoring canvas input.
            ImVec2 close;
            if (const auto* window = ImGui::FindWindowByName(inspection.c_str()))
                close = {window->Pos.x + window->Size.x - 14,
                         window->Pos.y + window->TitleBarHeight * .5f};
            click(close.x, close.y);
            settle();
            Check(!ImGui::FindWindowByName(inspection.c_str())->Active,
                  "inspection close button failed");
            Check(studio.Status().viewers_ <= 8, "inspection exceeded the shared preview budget");
        }
        Activate("###output", "###canvas.edit");
        settle();
        if (scene_mode) {
            const auto image = OutputRect();
            click(image.Min.x + image.GetWidth() * .35f, image.Min.y + image.GetHeight() * .65f);
            Check(studio.Workflow().selected_author_node_ == 2,
                  "clicking rendered cube did not select its author transform");
            settle();
        }
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
        if (automation_mode) {
            capture("driven");
            const auto before_freeze = studio.Workflow().requested_generation_;
            Activate("###output", "###transform_driver.title");
            settle();
            Activate("###output", "###transform_driver.freeze");
            ready();
            Check(studio.Workflow().requested_generation_ > before_freeze,
                  "explicit driver freeze did not compile an authored change");
        }
        capture("before");
        const auto rect = OutputRect();
        ImGui::GetIO().AddMousePosEvent(rect.GetCenter().x, rect.GetCenter().y);
        frame();
        ImGui::GetIO().AddMouseButtonEvent(0, true);
        frame();
        const auto generation = studio.Workflow().requested_generation_;
        for (int step = 1; step <= 6; ++step) {
            ImGui::GetIO().AddMousePosEvent(rect.GetCenter().x + rect.GetWidth() * .2f * step / 6,
                                            rect.GetCenter().y);
            frame();
        }
        ImGui::GetIO().AddMouseButtonEvent(0, false);
        frame();
        ready();
        Check(studio.Workflow().requested_generation_ > generation,
              "drag did not compile new output");
        capture("moved");
        Activate("###graph", "###save");
        settle();
        Activate("###graph", "###publish");
        for (int index = 0; index < 150 && !std::filesystem::exists(package); ++index) frame();
        Check(std::filesystem::exists(package), "edited canvas publication missing");
        const auto saved = project::Load(path).snapshot_;
        const auto published = project::LoadPackage(package);
        const auto expected_translation = scene_mode ? .2 * 320 / 180 : .2;
        Check(std::abs(graph::Scalar(saved.document_.nodes_[1], "translate_x", 0) -
                       expected_translation) < .005 &&
                      saved.document_.edges_.size() == (named_mode   ? 0
                                                        : scene_mode ? 6
                                                                     : 2) &&
                      saved.document_.nodes_.size() == (scene_mode        ? 7
                                                        : automation_mode ? 4
                                                                          : 3),
              "actual Studio drag not saved or altered graph connections");
        if (named_mode)
            Check(saved.document_.bindings_ == initial_saved.bindings_ &&
                          saved.document_.signals_ == initial_saved.signals_,
                  "canvas edit replaced authored named routes");
        Check(project::EncodeProgram(published.program_) ==
                      project::EncodeProgram(std::get<graph::ExecutionPlan>(
                              graph::Compile(saved.document_, registry))),
              "published program differs from saved canvas");
        Activate("###graph", "###undo");
        capture("undone");
        Activate("###graph", "###reopen");
        const auto old_generation = studio.Workflow().installed_generation_;
        for (int index = 0;
             index < 150 && studio.Workflow().installed_generation_ == old_generation; ++index)
            frame();
        Check(studio.Workflow().installed_generation_ != old_generation,
              "reopen retained previous plan");
        capture("reopened");
        std::cout << "Studio author selection, actual drag, new output, save/publish/undo/reopen: "
                  << root << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
