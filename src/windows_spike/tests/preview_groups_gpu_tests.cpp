#include <bgfx/bgfx.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <chrono>
#include <iostream>
#include <set>

#include "preview_routing.h"
#include "rhythm/project/store.h"
#include "rhythm/studio/studio.h"

namespace {
using namespace rhythm;
void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
editor::Snapshot Fixture() {
    graph::Registry registry;
    editor::Snapshot snapshot;
    snapshot.document_.id_ = "preview.groups";
    snapshot.document_.canvas_ = {320, 180};
    snapshot.document_.nodes_ = {
            registry.MakeNode(1, "core.time"), registry.MakeNode(2, "texture.gradient"),
            registry.MakeNode(3, "texture.trail"), registry.MakeNode(4, "texture.display"),
            registry.MakeNode(5, "output.texture")};
    snapshot.document_.edges_ = {
            {1, 1, 2, "amount"}, {2, 2, 3, "source"}, {3, 3, 4, "source"}, {4, 4, 5, "source"}};
    snapshot.document_.output_ = 5;
    for (graph::NodeId id = 6; id <= 40; ++id)
        snapshot.document_.nodes_.push_back(registry.MakeNode(id, "texture.gradient"));
    for (graph::NodeId id = 1; id <= 40; ++id)
        snapshot.positions_[id] = {float((id - 1) % 7) * 260, float((id - 1) / 7) * 290};
    return snapshot;
}
void CheckOutputContinuity(const graph::Document& document, render::Renderer& renderer) {
    graph::Registry registry;
    runtime::Runtime stable, paged;
    const auto baseline = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
    std::vector<graph::NodeId> nodes;
    for (const auto& node : document.nodes_) nodes.push_back(node.id_);
    studio::PreviewRouting routing;
    auto request = routing.Prepare(nodes, {}, document.id_);
    auto plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry, request.roots_));
    render::Readback first, second;
    for (int frame = 0; frame < 60; ++frame) {
        if (frame && frame % 10 == 0) {
            routing.StepPage(1);
            request = routing.Prepare(nodes, {}, document.id_);
            plan = std::get<graph::ExecutionPlan>(
                    graph::Compile(document, registry, request.roots_));
        }
        renderer.BeginFrame();
        const runtime::FrameContext context{frame / 60.0, 0, {64, 64}, true};
        const auto a = stable.EvaluateSafely(baseline, context, renderer);
        const auto b = paged.EvaluateSafely(plan, context, renderer);
        Check(!a.budget_ && !b.budget_, "paged final output within budget");
        if (frame == 59) {
            first = renderer.RequestReadback(a.final_);
            second = renderer.RequestReadback(b.final_);
        }
        renderer.EndFrame();
    }
    std::optional<render::ReadbackImage> a, b;
    for (int frame = 0; frame < 16 && (!a || !b); ++frame) {
        if (!a) a = first.Poll();
        if (!b) b = second.Poll();
        renderer.BeginFrame();
        renderer.EndFrame();
    }
    Check(a && b && a->rgba_ == b->rgba_,
          "final trail history unchanged by preview group compilation");
    bool color = false;
    for (std::size_t offset = 0; offset < a->rgba_.size(); offset += 4)
        color |= a->rgba_[offset] || a->rgba_[offset + 1] || a->rgba_[offset + 2];
    Check(color, "continuity comparison is nonblack");
}
void NextGroup() {
    // Checked borrowed ImGui window only in this synchronous test adapter.
    const auto* window = ImGui::FindWindowByName("###graph");
    Check(window != nullptr, "graph window");
    const auto seed = ImHashStr("preview.navigation", 0, window->ID);
    ImGui::ActivateItemByID(ImHashStr("###next", 0, seed));
}
}  // namespace
int main(int argc, char* argv[]) {
    try {
        Check(argc == 3, "preview_groups resources output");
        const auto output =
                std::filesystem::path(argv[2]) /
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        const auto snapshot = Fixture();
        project::Save(output, snapshot);
        const auto saved = project::Load(output).snapshot_;
        platform::Host host(true);
        host.Resize({1920, 1080});
        ImGui::GetIO().IniFilename = nullptr;
        auto renderer = host.CreateRenderer();
        auto font = host.CreateFontTexture(renderer);
        CheckOutputContinuity(snapshot.document_, renderer);
        studio::Studio studio(argv[1], output);
        int ready = -1;
        std::size_t groups = 0;
        std::set<std::size_t> visited;
        for (int frame = 0; frame < 360; ++frame) {
            Check(host.Poll(), "preview groups host closed");
            host.BeginUi();
            renderer.BeginFrame();
            if (ready >= 0 && frame > ready && (frame - ready) % 30 == 0) NextGroup();
            studio.Frame(host, renderer, frame / 60.0);
            const auto status = studio.Status();
            if (ready < 0 && frame > 30 && status.preview_groups_ >= 3 && status.inline_previews_) {
                ready = frame;
                groups = status.preview_groups_;
            }
            if (ready >= 0 && (frame - ready) % 30 >= 10 && status.inline_previews_)
                visited.insert(status.preview_group_);
            Check(!status.budget_limited_ && status.viewers_ + status.signal_previews_ <= 8 &&
                          status.authored_nodes_ == 40,
                  "Studio preview budget and graph integrity");
            renderer.Submit({}, host.EndUi(), 0x111822ff);
            if (ready >= 0 && frame == ready + 75) {
                const auto capture = (output / "preview-groups").string();
                bgfx::requestScreenShot(BGFX_INVALID_HANDLE, capture.c_str());
            }
            renderer.EndFrame();
            if (ready >= 0 && visited.size() == groups) break;
        }
        Check(ready >= 0 && groups >= 3 && visited.size() == groups && studio.HasValidPlan(),
              "all actual Studio preview groups reachable");
        Check(project::Load(output).snapshot_ == saved,
              "view navigation leaves saved project unchanged");
        std::cout << "40-node Studio visited " << groups
                  << " groups; budget=8; trail output pixels match uninterrupted baseline\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
