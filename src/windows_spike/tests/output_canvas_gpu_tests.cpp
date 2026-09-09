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
        // Use actual graph selection, including the existing auto-fit mapping.
        for (int y = 300; y < 950 && studio.Workflow().selected_author_node_ != 2; y += 60)
            for (int x = 50; x < 790 && studio.Workflow().selected_author_node_ != 2; x += 60)
                click(float(x), float(y));
        Check(studio.Workflow().selected_author_node_ == 2, "could not pick affine author node");
        ready();
        Activate("###output", "###canvas.edit");
        settle();
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
        Check(std::abs(graph::Scalar(saved.document_.nodes_[1], "translate_x", 0) - .2) < .005 &&
                      saved.document_.edges_.size() == 2 && saved.document_.nodes_.size() == 3,
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
