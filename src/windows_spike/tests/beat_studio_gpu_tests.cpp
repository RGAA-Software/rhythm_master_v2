#include <bgfx/bgfx.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <chrono>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

#include "rhythm/project/package.h"
#include "rhythm/project/store.h"
#include "rhythm/studio/studio.h"
#include "workflow_evidence.h"

namespace {
void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
// Checked ImGui borrows stay inside the synchronous graphics test adapter.
void Activate(const char* window_name, const std::string& label, bool controls = false) {
    const auto* window = ImGui::FindWindowByName(window_name);
    Check(window != nullptr, "beat UI window missing");
    const auto scope = controls ? ImHashStr("public_controls", 0, window->ID) : window->ID;
    ImGui::ActivateItemByID(ImHashStr(label.c_str(), 0, scope));
}
void Place(const char* name, ImVec2 position, ImVec2 size) {
    if (auto* window = ImGui::FindWindowByName(name)) {
        ImGui::SetWindowDock(window, 0, ImGuiCond_Always);
        ImGui::SetWindowPos(window, position, ImGuiCond_Always);
        ImGui::SetWindowSize(window, size, ImGuiCond_Always);
    }
}
}  // namespace
int main(int argc, char* argv[]) {
    using namespace rhythm;
    try {
        Check(argc == 3, "beat_studio resources output");
        const std::filesystem::path resources(argv[1]), root(argv[2]);
        const auto path = root / "Projects/beat.rhythmproj";
        const auto package = root / "Published/beat.rhythmpack";
        graph::Registry registry;
        editor::Snapshot initial;
        initial.title_ = "Quantized color acceptance";
        auto& document = initial.document_;
        document.id_ = "beat-studio-acceptance";
        document.canvas_ = {320, 180};
        document.nodes_ = {
                registry.MakeNode(1, "control.scalar"), registry.MakeNode(2, "texture.gradient"),
                registry.MakeNode(3, "texture.gradient"), registry.MakeNode(4, "texture.composite"),
                registry.MakeNode(5, "output.texture")};
        document.nodes_[0].properties_["value"] = 0.0;
        for (const auto key : {"color_a", "color_b"}) {
            document.nodes_[1].properties_[key] = graph::Color{1, 0, 0, 1};
            document.nodes_[2].properties_[key] = graph::Color{0, 0, 1, 1};
        }
        document.edges_ = {
                {1, 2, 4, "a"}, {2, 3, 4, "b"}, {3, 1, 4, "amount"}, {4, 4, 5, "source"}};
        document.output_ = 5;
        document.control_snapshots_ = {{1, "Blue", {{1, 1}}}};
        project::Save(path, initial);
        std::ifstream locale(resources / "locales/zh-CN/studio.json");
        const auto text = nlohmann::json::parse(locale).get<std::map<std::string, std::string>>();
        platform::Host host(true);
        host.Resize({1600, 1100});
        ImGui::GetIO().IniFilename = nullptr;
        auto renderer = host.CreateRenderer();
        auto font = host.CreateFontTexture(renderer);
        studio::Studio studio(resources, path);
        testing::WorkflowEvidence evidence(root);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
        double seconds = 0;
        const auto frame = [&] {
            Check(host.Poll() && std::chrono::steady_clock::now() < deadline,
                  "beat UI timeout/closed");
            host.BeginUi();
            renderer.BeginFrame();
            Place("###inspector", {950, 0}, {640, 1090});
            Place("###output", {10, 700}, {900, 390});
            Place("###timeline", {10, 400}, {900, 280});
            studio.Frame(host, renderer, seconds);
            renderer.Submit({}, host.EndUi(), 0x111822ff);
            renderer.EndFrame();
            evidence.Record("beat-ui", studio);
        };
        const auto settle = [&] {
            for (int index = 0; index < 4; ++index) frame();
        };
        for (int index = 0; index < 100 && !studio.HasValidPlan(); ++index) frame();
        Check(studio.HasValidPlan(), "beat fixture compile failed");
        settle();
        Activate("###inspector", "###beat.title");
        settle();
        Activate("###inspector", "###beat.enabled");
        settle();
        for (int index = 0; index < 100 && !studio.HasValidPlan(); ++index) frame();
        Check(studio.HasValidPlan(), "grid edit compile failed");
        Activate("###inspector", "###beat.quantization");
        settle();
        {
            const auto& context = *ImGui::GetCurrentContext();
            Check(!context.OpenPopupStack.empty(), "quantization popup missing");
            const auto* popup = context.OpenPopupStack.back().Window;
            Check(popup != nullptr, "quantization popup not drawn");
            ImGui::ActivateItemByID(ImHashStr(text.at("beat.next_beat").c_str(), 0, popup->ID));
        }
        settle();
        Activate("###graph", "###timeline");
        settle();
        Activate("###timeline", text.at("timeline.pause"));
        settle();
        const auto generation = studio.Workflow().installed_generation_;
        Activate("###inspector", "###controls.recall", true);
        settle();
        const auto capture = [&](const char* name) {
            const auto* window = ImGui::FindWindowByName("###output");
            Check(window != nullptr, "output window missing");
            std::ofstream metadata(root / (std::string(name) + ".json"));
            metadata << nlohmann::json{{"x", window->Pos.x + window->Size.x / 2},
                                       {"y", window->Pos.y + window->Size.y / 2},
                                       {"seconds", seconds},
                                       {"generation", generation}};
            bgfx::requestScreenShot(BGFX_INVALID_HANDLE, (root / name).string().c_str());
            settle();
        };
        seconds = 0.8;
        capture("paused");
        Activate("###timeline", text.at("timeline.play"));
        settle();
        // Existing local clock remains frozen at zero through pause. Advance in
        // normal-sized steps, then inspect both sides of the target boundary.
        for (int index = 1; index <= 49; ++index) {
            seconds = 0.8 + index * 0.01;
            frame();
        }
        capture("before");
        seconds = 1.3;
        capture("after");
        Check(studio.HasValidPlan() && !studio.Status().budget_limited_ &&
                      studio.Workflow().installed_generation_ == generation,
              "live recall invalidated current render plan");
        Activate("###graph", "###save");
        settle();
        Activate("###graph", "###publish");
        for (int index = 0; index < 150 && !std::filesystem::exists(package); ++index) frame();
        const auto saved = project::Load(path).snapshot_;
        const auto published = project::LoadPackage(package);
        Check(saved.document_.beat_grid_ == parameters::BeatSettings{} &&
                      published.program_.beat_grid_ == saved.document_.beat_grid_ &&
                      graph::Scalar(saved.document_.nodes_[0], "value", -1) == 0,
              "grid save/publication or transient default changed");
        Activate("###graph", "###reopen");
        settle();
        for (int index = 0; index < 150 && (!studio.HasValidPlan() ||
                                            studio.Workflow().installed_generation_ == generation);
             ++index)
            frame();
        Check(studio.HasValidPlan() && studio.Workflow().installed_generation_ != generation,
              "reopen retained the previous plan");
        capture("reopened");
        std::cout << "Studio grid edit, paused quantized recall and save/publication: " << root
                  << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
