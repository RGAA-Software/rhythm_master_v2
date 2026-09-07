#include <bgfx/bgfx.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <iostream>
#include <stdexcept>

#include "rhythm/project/store.h"
#include "rhythm/studio/studio.h"

namespace {
// Borrow ImGui windows only inside these synchronous UI boundary queries.
void Activate(const char* window_name, const char* label, const char* scope = "") {
    const auto* window = ImGui::FindWindowByName(window_name);
    if (!window) throw std::runtime_error("component_preview.window");
    const auto seed = *scope ? ImHashStr(scope, 0, window->ID) : window->ID;
    ImGui::ActivateItemByID(ImHashStr(label, 0, seed));
}
void ZoomComponent() {
    const auto* window = ImGui::FindWindowByName("###component.workbench");
    if (!window || !window->Active) throw std::runtime_error("component_preview.not_open");
    ImGui::GetIO().AddMousePosEvent(window->Pos.x + 370, window->Pos.y + 370);
    ImGui::GetIO().AddMouseWheelEvent(0, 1);
}
}  // namespace
int main(int argc, char* argv[]) {
    using namespace rhythm;
    try {
        if (argc != 4) throw std::invalid_argument("component_preview resources template output");
        const std::filesystem::path resources(argv[1]), source(argv[2]), output(argv[3]);
        const auto prepared = project::PrepareTemplate(source, output / "assets");
        project::Save(output, prepared.snapshot_);
        platform::Host host(true);
        host.Resize({1920, 1080});
        ImGui::GetIO().IniFilename = nullptr;
        auto renderer = host.CreateRenderer();
        auto font = host.CreateFontTexture(renderer);
        studio::Studio studio(resources, output);
        std::size_t maximum_previews = 0;
        std::size_t maximum_signals = 0;
        bool captured = false;
        std::uint64_t peak_bytes = 0;
        for (int frame = 0; frame < 210; ++frame) {
            if (!host.Poll()) throw std::runtime_error("component_preview.closed");
            if (!studio.Status().component_inline_previews_ && frame >= 55 && frame < 140 &&
                frame % 5 == 0)
                ZoomComponent();
            host.BeginUi();
            renderer.BeginFrame();
            if (frame == 15) Activate("###inspector", "###component.panel");
            if (frame == 30)
                Activate("###inspector", "编辑内部网络", "component.official.resonance_core");
            if (frame == 180) Activate("###component.workbench", "取消草稿");
            studio.Frame(host, renderer, frame / 60.0);
            auto draw = host.EndUi();
            renderer.Submit({}, draw, 0x111822ff);
            if (!captured && frame >= 150 && frame < 175 &&
                studio.Status().component_inline_previews_ && studio.Status().signal_previews_) {
                const auto capture = (output / "component-preview").string();
                bgfx::requestScreenShot(BGFX_INVALID_HANDLE, capture.c_str());
                captured = true;
            }
            renderer.EndFrame();
            const auto status = studio.Status();
            maximum_previews = std::max(maximum_previews, status.component_inline_previews_);
            maximum_signals = std::max(maximum_signals, status.signal_previews_);
            peak_bytes = std::max(peak_bytes, renderer.Stats().texture_bytes_);
            if (status.budget_limited_ || status.viewers_ + status.signal_previews_ > 8 ||
                status.authored_nodes_ != 9)
                throw std::runtime_error("component_preview.budget_or_draft_mutation");
            if (frame > 185 && status.component_inline_previews_ != 0)
                throw std::runtime_error("component_preview.cancel_keeps_viewers");
        }
        if (!captured || !maximum_previews || !maximum_signals || !studio.HasValidPlan())
            throw std::runtime_error("component_preview.no_internal_images");
        std::cout << "actual Studio component previews=" << maximum_previews
                  << " numeric previews=" << maximum_signals
                  << " total viewer budget=8 peak_texture_bytes=" << peak_bytes
                  << " frames=210 cancel/draft isolation passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
