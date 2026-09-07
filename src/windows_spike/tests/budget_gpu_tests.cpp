#include <imgui.h>
#include <imgui_internal.h>

#include <chrono>
#include <iostream>
#include <stdexcept>

#include "rhythm/platform/host.h"
#include "rhythm/project/store.h"
#include "rhythm/runtime/runtime.h"
#include "rhythm/studio/studio.h"

namespace {
void ActivateProfile() {
    // Synchronous borrowed ImGui window; never stored by project code.
    for (auto* window : ImGui::GetCurrentContext()->Windows)
        if (std::string_view(window->Name).find("###inspector") != std::string_view::npos) {
            ImGui::SetScrollY(window, window->ScrollMax.y);
            ImGui::ActivateItemByID(window->GetID("###profile"));
            return;
        }
    throw std::runtime_error("profile.inspector_missing");
}
void ActivateReopen() {
    // ImGui's borrowed window is inspected only within this synchronous adapter.
    for (auto* window : ImGui::GetCurrentContext()->Windows)
        if (std::string_view(window->Name).find("###graph") != std::string_view::npos) {
            ImGui::ActivateItemByID(window->GetID("###reopen"));
            return;
        }
    throw std::runtime_error("budget.graph_window_missing");
}
}  // namespace

int main(int argc, char* argv[]) {
    using namespace rhythm;
    try {
        if (argc != 3) throw std::invalid_argument("budget_gpu resources output_root");
        const std::filesystem::path resources(argv[1]);
        const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto project = std::filesystem::path(argv[2]) / std::to_string(unique);
        auto prepared = project::PrepareTemplate(resources / "content/templates/resonance_gate",
                                                 project / "assets");
        prepared.snapshot_.document_.canvas_ = {1024, 1024};
        project::Save(project, prepared.snapshot_);
        platform::Host host(true);
        host.Resize({1280, 720});
        auto renderer = host.CreateRenderer();
        auto font = host.CreateFontTexture(renderer);
        // Dynamic intermediates now fit by themselves. Simulate concurrent GPU
        // consumers so this remains an admission/recovery test as reuse improves.
        auto pressure =
                renderer.CreateTexture({4096, 4096}, {}, render::TexturePrecision::kFloat16);
        {
            studio::Studio studio(resources, project);
            std::size_t limited = 0, recovered = 0, profiled = 0;
            for (int frame = 0; frame < 240; ++frame) {
                if (!host.Poll()) throw std::runtime_error("budget.window_closed");
                if (frame == 120) {
                    if (limited < 60) throw std::runtime_error("budget.not_exercised");
                    pressure = {};
                    prepared.snapshot_.document_.canvas_ = {640, 360};
                    ++prepared.snapshot_.document_.revision_;
                    project::Save(project, prepared.snapshot_);
                    ActivateReopen();
                }
                if (frame == 160) ActivateProfile();
                host.BeginUi();
                renderer.BeginFrame();
                studio.Frame(host, renderer, frame / 60.0);
                const auto status = studio.Status();
                limited += status.budget_limited_;
                if (frame > 165 && status.profiled_nodes_ == 164) ++profiled;
                if (frame > 150 && !status.budget_limited_ &&
                    renderer.Stats().texture_bytes_ > 40ULL * 1024 * 1024 &&
                    renderer.Stats().texture_bytes_ < 100ULL * 1024 * 1024)
                    ++recovered;
                renderer.Submit({}, host.EndUi());
                renderer.EndFrame();
            }
            if (profiled < 60) throw std::runtime_error("profile.no_node_measurements");
            if (recovered < 60) throw std::runtime_error("budget.editor_did_not_recover");
            std::cout << "editor_frames=240 limited=" << limited << " recovered=" << recovered
                      << " profiled=" << profiled << '\n';
        }
        // Tiny targets exercise the backend handle pool independently of byte limits.
        graph::Registry registry;
        graph::Document document;
        document.id_ = "gpu.pass_budget";
        document.nodes_.push_back(registry.MakeNode(1, "texture.gradient"));
        for (graph::NodeId id = 2; id <= 260; ++id) {
            document.nodes_.push_back(registry.MakeNode(id, "texture.blend"));
            document.edges_.push_back({id * 2, id - 1, id, "a"});
            document.edges_.push_back({id * 2 + 1, id - 1, id, "b"});
        }
        document.output_ = 261;
        document.nodes_.push_back(registry.MakeNode(261, "output.texture"));
        document.edges_.push_back({522, 260, 261, "source"});
        const auto plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
        runtime::Runtime runtime;
        for (int frame = 0; frame < 3; ++frame) {
            host.Poll();
            host.BeginUi();
            renderer.BeginFrame();
            const auto result = runtime.EvaluateSafely(plan, {0, 0, {16, 16}}, renderer);
            if (result.budget_ != render::Budget::kBackendResources)
                throw std::runtime_error("budget.gpu_handles_not_exercised");
            ImGui::Begin("Budget recovery");
            ImGui::TextUnformatted("Editor remains available after render budget exhaustion.");
            ImGui::End();
            renderer.Submit({}, host.EndUi());
            renderer.EndFrame();
        }
        auto target = renderer.CreateTexture({16, 16});
        host.BeginUi();
        renderer.BeginFrame();
        render::DrawList draw;
        draw.width_ = draw.height_ = 16;
        for (int pass = 0; pass < 240; ++pass) renderer.Submit(target.Handle(), draw);
        bool rejected = false;
        try {
            renderer.Submit(target.Handle(), draw);
        } catch (const render::BudgetExceeded& error) {
            rejected = error.Kind() == render::Budget::kPasses;
        }
        if (!rejected) throw std::runtime_error("budget.gpu_pass_not_exercised");
        ImGui::Begin("Presentation reserve");
        ImGui::TextUnformatted("UI still renders after 240 effect passes.");
        ImGui::End();
        renderer.Submit({}, host.EndUi());
        renderer.EndFrame();
        std::cout << "gpu_handle_and_pass_budget_recovery=passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
