#include <bgfx/bgfx.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <chrono>
#include <iostream>
#include <stdexcept>

#include "rhythm/project/package.h"
#include "rhythm/project/store.h"
#include "rhythm/studio/studio.h"

namespace {
void Activate(const char* window_name, const char* item) {
    const auto* window = ImGui::FindWindowByName(window_name);
    if (!window) throw std::runtime_error("soundtrack UI window missing");
    ImGui::ActivateItemByID(ImHashStr(item, 0, window->ID));
}
}  // namespace
int main(int argc, char* argv[]) {
    using namespace rhythm;
    try {
        if (argc != 5)
            throw std::invalid_argument("soundtrack_studio resources template music output");
        const auto root =
                std::filesystem::path(argv[4]) /
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        const auto project_path =
                root / "Projects" / std::filesystem::path(u8"音乐作品.rhythmproj");
        const auto package_path =
                root / "Published" / std::filesystem::path(u8"音乐作品.rhythmpack");
        project::Save(project_path,
                      project::PrepareTemplate(argv[2], project_path / "assets").snapshot_);
        platform::Host host(true);
        host.Resize({1920, 1440});
        ImGui::GetIO().IniFilename = nullptr;
        auto renderer = host.CreateRenderer();
        auto font = host.CreateFontTexture(renderer);
        studio::Studio studio(argv[1], project_path);
        studio.LoadAudioFile(argv[3], 0);
        const auto start = std::chrono::steady_clock::now();
        int published = -1;
        bool saved = false, observed_clear = false, restored = false;
        int frames = 0;
        for (; frames < 1200; ++frames) {
            if (!host.Poll() || std::chrono::steady_clock::now() - start > std::chrono::seconds(40))
                throw std::runtime_error("soundtrack Studio workflow timeout");
            host.BeginUi();
            renderer.BeginFrame();
            if (frames == 10) {
                // Make the inspector visible at full height, equivalent to
                // undocking/resizing it in the normal editor.
                if (auto* window = ImGui::FindWindowByName("###inspector")) {
                    ImGui::SetWindowDock(window, 0, ImGuiCond_Always);
                    ImGui::SetWindowPos(window, {1300, 20}, ImGuiCond_Always);
                    ImGui::SetWindowSize(window, {600, 1380}, ImGuiCond_Always);
                }
                if (auto* window = ImGui::FindWindowByName("###output")) {
                    ImGui::SetWindowDock(window, 0, ImGuiCond_Always);
                    ImGui::SetWindowPos(window, {650, 1030}, ImGuiCond_Always);
                    ImGui::SetWindowSize(window, {620, 360}, ImGuiCond_Always);
                }
            }
            if (frames == 20) Activate("###inspector", "###music.bind");
            if (!saved && frames >= 45 && frames % 30 == 15) Activate("###graph", "###save");
            if (saved && published < 0 && frames % 30 == 20) Activate("###graph", "###publish");
            if (published >= 0 && frames == published + 5)
                Activate("###inspector", "###music.clear");
            if (published >= 0 && frames == published + 20) Activate("###graph", "###reopen");
            studio.Frame(host, renderer,
                         std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
                                 .count());
            renderer.Submit({}, host.EndUi(), 0x111822ff);
            if (published >= 0 && frames == published + 210) {
                const auto path = (root / "soundtrack-studio").string();
                bgfx::requestScreenShot(BGFX_INVALID_HANDLE, path.c_str());
            }
            renderer.EndFrame();
            if (studio.Status().budget_limited_)
                throw std::runtime_error("soundtrack graph budget");
            if (!saved && frames > 45 && frames % 10 == 0)
                saved = project::Load(project_path).snapshot_.soundtrack_.has_value();
            if (published < 0 && std::filesystem::is_regular_file(package_path)) {
                const auto package = project::LoadPackage(package_path);
                if (!package.soundtrack_ ||
                    package.profile_ != project::PackageProfile::kMusicPerformanceV1)
                    throw std::runtime_error("toolbar publish lost music");
                published = frames;
            }
            if (published >= 0 && frames == published + 15)
                observed_clear = studio.Status().audio_rms_ == 0;
            if (published >= 0 && frames > published + 25 && studio.Status().audio_rms_ > 0)
                restored = true;
            if (published >= 0 && frames > published + 220) break;
        }
        if (!saved || published < 0 || !observed_clear || !restored || !studio.HasValidPlan() ||
            studio.Status().authored_nodes_ != 9)
            throw std::runtime_error("Studio bind/save/publish/clear/reopen did not complete");
        const auto path = package_path.u8string();
        std::cout << "Studio bind/save/publish/clear/reopen: music restored, 197 instructions, "
                     "frames="
                  << frames << " package=" << std::string(path.begin(), path.end()) << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
