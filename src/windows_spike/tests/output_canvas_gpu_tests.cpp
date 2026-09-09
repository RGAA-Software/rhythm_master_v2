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
        Check(argc == 3 || argc == 4, "resources output [--scene]");
        const bool scene_mode = argc == 4 && std::string_view(argv[3]) == "--scene";
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
        project::Save(path, initial);
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
            // Use actual graph selection, including the existing auto-fit mapping.
            for (int y = 300; y < 950 && studio.Workflow().selected_author_node_ != 2; y += 60)
                for (int x = 50; x < 790 && studio.Workflow().selected_author_node_ != 2; x += 60)
                    click(float(x), float(y));
            Check(studio.Workflow().selected_author_node_ == 2,
                  "could not pick affine author node");
        }
        ready();
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
                      saved.document_.edges_.size() == (scene_mode ? 6 : 2) &&
                      saved.document_.nodes_.size() == (scene_mode ? 7 : 3),
              "actual Studio drag not saved or altered graph connections");
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
