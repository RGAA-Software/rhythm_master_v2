#include <imgui.h>

#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <string_view>

#include "rhythm/audio_ui/audio_panel.h"
#include "rhythm/platform/host.h"
#include "rhythm/player/package_loader.h"
#include "rhythm/player/session.h"
#include "rhythm/render/layout.h"

// Native process arguments are borrowed at the entry boundary only.
#ifdef _WIN32
int wmain(int argc, wchar_t* argv[]) {
#else
int main(int argc, char* argv[]) {
#endif
    try {
        bool smoke = false;
        std::optional<std::filesystem::path> requested_package;
        std::optional<std::filesystem::path> requested_audio;
        for (int index = 1; index < argc; ++index) {
            const std::filesystem::path argument(argv[index]);
            if (argument == "--smoke")
                smoke = true;
            else if (argument == "--package" && index + 1 < argc)
                requested_package = std::filesystem::path(argv[++index]);
#ifdef RHYTHM_HAS_LOCAL_MEDIA
            else if (argument == "--audio" && index + 1 < argc)
                requested_audio = std::filesystem::path(argv[++index]);
#endif
            else
                throw std::invalid_argument(
                        "Usage: rhythm_player [--package file] [--audio local-file] [--smoke]");
        }
        rhythm::platform::Host host(smoke);
        auto renderer = host.CreateRenderer();
        auto font = host.CreateFontTexture(renderer);
        rhythm::player::Session session;
        rhythm::player::PackageLoader package_loader;
        rhythm::audio_ui::AudioPanel audio_panel;
        const auto set_paused = [&](bool paused) {
            audio_panel.ApplyPlayback({paused, {}});
            session.SetPaused(paused);
        };
        const auto restart = [&] {
            audio_panel.ApplyPlayback({{}, 0});
            session.Restart();
        };
#ifdef RHYTHM_HAS_LOCAL_MEDIA
        if (smoke) audio_panel.SetVolume(0);
        if (requested_audio) audio_panel.LoadFile(*requested_audio);
#endif
        std::map<std::string, std::map<std::string, std::string>> catalogs;
        for (const auto locale : {"zh-CN", "en-US"}) {
            std::ifstream file(host.ResourceDirectory() / "locales" / locale / "studio.json");
            catalogs[locale] =
                    nlohmann::json::parse(file).get<std::map<std::string, std::string>>();
        }
        session.Open(requested_package.value_or(host.ResourceDirectory() /
                                                "content/packages/signal_texture.rhythmpack"));
        std::array<char, 4096> path{};
        std::string error;
        bool chinese = true;
        const auto start = std::chrono::steady_clock::now();
        std::uint64_t frames = 0;
        bool observed_audio = false;
        while (host.Poll() && (!smoke || frames < 30)) {
            const auto elapsed =
                    std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
            if (host.IsSuspended()) {
                audio_panel.SetSuspended(true);
                session.Tick(elapsed, true, {}, renderer);
                continue;
            }
            audio_panel.SetSuspended(false);
            host.BeginUi();
            renderer.BeginFrame();
            // Runtime changes happen before this frame borrows any texture handles.
            if (auto result = package_loader.Take()) {
                if (result->package_) {
                    session.LoadPrepared(std::move(*result->package_));
                    error.clear();
                } else if (result->error_ != rhythm::player::PackageLoadError::kCancelled) {
                    error = "package_error";
                }
            }
            if (!ImGui::GetIO().WantTextInput) {
                if (ImGui::IsKeyPressed(ImGuiKey_Space)) set_paused(!session.Paused());
                if (ImGui::IsKeyPressed(ImGuiKey_R)) restart();
            }
            ImGui::SetNextWindowPos({0, 0});
            ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
            ImGui::Begin("Player", nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoSavedSettings);
            ImGui::TextUnformatted(session.Title().c_str());
            ImGui::SameLine();
            if (ImGui::Button(chinese ? "English" : "简体中文")) chinese = !chinese;
            if (ImGui::Button(session.Paused() ? (chinese ? "继续##play" : "Resume##play")
                                               : (chinese ? "暂停##play" : "Pause##play")))
                set_paused(!session.Paused());
            ImGui::SameLine();
            if (ImGui::Button(chinese ? "重新播放##restart" : "Restart##restart")) restart();
            ImGui::SameLine();
            ImGui::Text("%.2f s", session.Seconds());
            audio_panel.Draw(catalogs.at(chinese ? "zh-CN" : "en-US"));
            ImGui::SetNextItemWidth(-140);
            ImGui::InputText(chinese ? "运行包路径##path" : "Package path##path", path.data(),
                             path.size());
            ImGui::BeginDisabled(package_loader.Busy());
            if (ImGui::Button(chinese ? "打开运行包##open" : "Open package##open")) {
                try {
                    const std::string utf8(path.data());
                    if (!package_loader.StartFile(
                                std::filesystem::path(std::u8string(utf8.begin(), utf8.end()))))
                        throw std::runtime_error("player.package_busy");
                    error.clear();
                } catch (const std::exception& failure) {
                    error = failure.what();
                }
            }
            ImGui::EndDisabled();
            if (package_loader.Busy()) {
                ImGui::SameLine();
                ImGui::TextUnformatted(chinese ? "正在打开运行包…" : "Opening package…");
                ImGui::SameLine();
                if (ImGui::Button(chinese ? "取消##cancel_open" : "Cancel##cancel_open"))
                    package_loader.Cancel();
            }
            if (!error.empty())
                ImGui::TextUnformatted(
                        chinese ? "无法打开运行包，继续播放当前内容。"
                                : "Cannot open package. Current playback is retained.");
            const auto available = ImGui::GetContentRegionAvail();
            const auto width = std::max(1.0f, available.x);
            const auto height = std::max(1.0f, available.y);
            rhythm::runtime::ExternalInputs inputs;
            const auto audio_frame = audio_panel.Frame();
            inputs.audio_ = audio_frame.features_;
            observed_audio |= inputs.audio_ && inputs.audio_->valid_ && inputs.audio_->rms_ > 0;
            const auto output =
                    session.Tick(smoke ? frames / 60.0 : elapsed, false, session.Canvas(), renderer,
                                 inputs, audio_frame.playback_);
            host.ClearViewerTextures();
            if (output.budget_)
                ImGui::TextWrapped("%s", catalogs.at(chinese ? "zh-CN" : "en-US")
                                                 .at("render.resource_budget")
                                                 .c_str());
            if (renderer.IsValid(output.final_)) {
                const auto fit = rhythm::render::AspectFit(session.Canvas(), {0, 0, width, height});
                const auto cursor = ImGui::GetCursorPos();
                ImGui::SetCursorPos({cursor.x + fit.x_, cursor.y + fit.y_});
                ImGui::Image(host.RegisterTexture(output.final_), {fit.width_, fit.height_});
            }
            ImGui::End();
            renderer.Submit({}, host.EndUi(), 0x111822ff);
            renderer.EndFrame();
            ++frames;
        }
        if (smoke && (frames != 30 || renderer.Stats().passes_ < 2))
            throw std::runtime_error("player.no_gpu_output");
        if (smoke && requested_audio && !observed_audio)
            throw std::runtime_error("player.no_file_audio_features");
        std::cout << "player_gpu_frames=" << frames << " package_loaded=" << session.Ready()
                  << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
