#include <bgfx/bgfx.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>

#include "rhythm/media/video_decoder.h"
#include "rhythm/project/store.h"
#include "rhythm/studio/studio.h"

namespace {
void Activate(const char* window_name, const char* item) {
    const auto* window = ImGui::FindWindowByName(window_name);
    if (!window) throw std::runtime_error("export UI window missing");
    ImGui::ActivateItemByID(ImHashStr(item, 0, window->ID));
}
}  // namespace
int main(int argc, char* argv[]) {
    using namespace rhythm;
    try {
        if (argc != 5) throw std::invalid_argument("export_ui resources template music output");
        const auto root =
                std::filesystem::path(argv[4]) /
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        const auto project_path =
                root / "Projects" / std::filesystem::path(u8"音画验收.rhythmproj");
        const auto output_path = root / "Exports" / std::filesystem::path(u8"音画验收.mp4");
        const auto prepared = project::PrepareTemplate(argv[2], project_path / "assets");
        project::Save(project_path, prepared.snapshot_);
        platform::Host host(true);
        host.Resize({1920, 1080});
        ImGui::GetIO().IniFilename = nullptr;
        auto renderer = host.CreateRenderer();
        auto font = host.CreateFontTexture(renderer);
        studio::Studio studio(argv[1], project_path);
        studio.LoadAudioFile(argv[3], 0);
        const auto start = std::chrono::steady_clock::now();
        const auto deadline = start + std::chrono::seconds(80);
        std::vector<double> frame_ms;
        int completed_frame = -1;
        for (int frame = 0;; ++frame) {
            if (!host.Poll() || std::chrono::steady_clock::now() > deadline)
                throw std::runtime_error("export UI did not complete");
            const auto frame_start = std::chrono::steady_clock::now();
            host.BeginUi();
            renderer.BeginFrame();
            if (frame == 20) Activate("###graph", "###export.open");
            if (frame == 25) Activate("###export.open", "###export.start");
            const auto elapsed = std::chrono::duration<double>(frame_start - start).count();
            studio.Frame(host, renderer, elapsed);
            const auto draw = host.EndUi();
            renderer.Submit({}, draw, 0x111822ff);
            if (completed_frame >= 0 && frame == completed_frame + 10) {
                const auto capture = (root / "export-ui").string();
                bgfx::requestScreenShot(BGFX_INVALID_HANDLE, capture.c_str());
            }
            renderer.EndFrame();
            if (frame > 25 && completed_frame < 0)
                frame_ms.push_back(std::chrono::duration<double, std::milli>(
                                           std::chrono::steady_clock::now() - frame_start)
                                           .count());
            if (studio.Status().budget_limited_)
                throw std::runtime_error("export blocked live graph budget");
            if (completed_frame < 0 && std::filesystem::is_regular_file(output_path))
                completed_frame = frame;
            if (completed_frame >= 0 && frame > completed_frame + 20) break;
        }
        if (!studio.HasValidPlan() ||
            studio.Status().authored_nodes_ != prepared.snapshot_.document_.nodes_.size() ||
            frame_ms.size() < 10)
            throw std::runtime_error("export mutated the live graph or prevented UI frames");
        media::VideoDecoder exported(output_path);
        int frames = 0;
        while (const auto frame = exported.Read()) {
            if (std::abs(frame->seconds_ - frames / 30.0) > 0.00001)
                throw std::runtime_error("UI export timestamps changed");
            ++frames;
        }
        if (frames != 480)
            throw std::runtime_error("UI did not adopt the full 16-second music duration");
        std::sort(frame_ms.begin(), frame_ms.end());
        const auto output_utf8 = output_path.u8string();
        std::cout << "Studio export button -> H264 MP4: 480 frames; parent frames="
                  << frame_ms.size() << " frame_ms_p50=" << frame_ms[frame_ms.size() / 2]
                  << " p95=" << frame_ms[frame_ms.size() * 95 / 100]
                  << " output=" << std::string(output_utf8.begin(), output_utf8.end()) << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
