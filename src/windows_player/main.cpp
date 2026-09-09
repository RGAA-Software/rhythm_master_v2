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
#include "rhythm/control_ui/beat_panel.h"
#include "rhythm/control_ui/control_panel.h"
#include "rhythm/platform/host.h"
#include "rhythm/player/package_loader.h"
#include "rhythm/project/store.h"
#include "rhythm/render/layout.h"
#include "scene_queue_panel.h"

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
        rhythm::player::SceneDeck deck;
        rhythm::player::SceneQueue scene_queue;
        rhythm::player_ui::SceneQueuePanel scene_panel;
        std::vector<rhythm::player_ui::SceneChoice> scene_choices;
        for (const auto& entry :
             rhythm::project::ScanTemplates(host.ResourceDirectory() / "content/templates"))
            scene_choices.push_back({host.ResourceDirectory() / "content/packages" /
                                             (entry.directory_.filename().string() + ".rhythmpack"),
                                     entry.titles_});
        rhythm::player::PackageLoader package_loader;
        rhythm::audio_ui::AudioPanel audio_panel;
        rhythm::control_ui::ControlPanel control_panel;
        rhythm::control_ui::BeatPanel beat_panel;
        const auto set_paused = [&](bool paused) {
            audio_panel.ApplyPlayback({paused, {}});
            deck.SetPaused(paused);
        };
        const auto restart = [&] {
            audio_panel.ApplyPlayback({{}, 0});
            deck.Restart();
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
        deck.Open(requested_package.value_or(host.ResourceDirectory() /
                                             "content/packages/signal_texture.rhythmpack"));
#ifdef RHYTHM_HAS_LOCAL_MEDIA
        const auto apply_soundtrack = [&] {
            if (const auto track = deck.Current().Soundtrack())
                audio_panel.LoadSoundtrack(*track);
            else
                audio_panel.ClearFile();
            if (smoke) audio_panel.SetVolume(0);
        };
        if (!requested_audio) apply_soundtrack();
#endif
        std::array<char, 4096> path{};
        std::string error;
        bool chinese = true;
        const auto start = std::chrono::steady_clock::now();
        std::uint64_t frames = 0;
        bool observed_audio = false;
        bool observed_gpu_output = false;
        while (host.Poll() && (!smoke || frames < 30)) {
            const auto elapsed =
                    std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
            if (host.IsSuspended()) {
                audio_panel.SetSuspended(true);
                deck.Tick(elapsed, true, rhythm::player::RenderQuality::kOriginal, renderer);
                continue;
            }
            audio_panel.SetSuspended(false);
            host.BeginUi();
            renderer.BeginFrame();
            // Runtime changes happen before this frame borrows any texture handles.
            if (auto result = package_loader.Take()) {
                if (result->package_) {
                    deck.LoadPrepared(std::move(*result->package_));
                    scene_queue.Clear();
                    beat_panel.Reset();
                    control_panel.Reset();
#ifdef RHYTHM_HAS_LOCAL_MEDIA
                    apply_soundtrack();
#endif
                    error.clear();
                } else if (result->error_ != rhythm::player::PackageLoadError::kCancelled) {
                    error = "package_error";
                }
            }
            scene_queue.Pump(deck.CanPrepareNext());
            if (!ImGui::GetIO().WantTextInput) {
                if (ImGui::IsKeyPressed(ImGuiKey_Space)) set_paused(!deck.Current().Paused());
                if (ImGui::IsKeyPressed(ImGuiKey_R)) restart();
            }
            ImGui::SetNextWindowPos({0, 0});
            ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
            ImGui::Begin("Player", nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoSavedSettings);
            ImGui::TextUnformatted(deck.Current().Title().c_str());
            ImGui::SameLine();
            if (ImGui::Button(chinese ? "English" : "简体中文")) chinese = !chinese;
            if (ImGui::Button(deck.Current().Paused() ? (chinese ? "继续##play" : "Resume##play")
                                                      : (chinese ? "暂停##play" : "Pause##play")))
                set_paused(!deck.Current().Paused());
            ImGui::SameLine();
            if (ImGui::Button(chinese ? "重新播放##restart" : "Restart##restart")) restart();
            ImGui::SameLine();
            ImGui::Text("%.2f s", deck.Current().Seconds());
            const auto& text = catalogs.at(chinese ? "zh-CN" : "en-US");
            if (const auto beat = beat_panel.Draw(deck.BeatGrid(), deck.Current().Seconds(), text);
                beat.changed_)
                deck.SetBeatGrid(beat.grid_);
            for (const auto kind : {rhythm::player::PerformanceActionKind::kSnapshot,
                                    rhythm::player::PerformanceActionKind::kNextScene}) {
                const auto& action = deck.ActionStatus(kind);
                if (rhythm::control_ui::DrawPerformanceAction(action, text))
                    deck.CancelAction(action.id_);
            }
            audio_panel.Draw(catalogs.at(chinese ? "zh-CN" : "en-US"));
            const auto edit = control_panel.Draw(
                    deck.Current().Controls(), deck.Current().CurrentControls(), text, false, true);
            if (edit.values_) deck.EditControls(*edit.values_);
            if (edit.recall_) deck.RequestSnapshot(*edit.recall_, beat_panel.Mode());
            if (deck.Current().ControlSequence()) {
                if (ImGui::Button(chinese ? "回到自动编排##follow_cues"
                                          : "Follow cues##follow_cues")) {
                    deck.FollowCues();
                    control_panel.Reset();
                }
                if (const auto active =
                            deck.Current().ControlSequence()->Active(deck.Current().Seconds()))
                    for (const auto& cue : deck.Current().ControlSequence()->Cues())
                        if (cue.id_ == *active) {
                            ImGui::SameLine();
                            ImGui::TextUnformatted(cue.title_.c_str());
                        }
            }
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
            scene_panel.Draw(scene_queue, deck, scene_choices, chinese ? "zh-CN" : "en-US",
                             catalogs.at(chinese ? "zh-CN" : "en-US"), beat_panel.Mode());
            const auto available = ImGui::GetContentRegionAvail();
            const auto width = std::max(1.0f, available.x);
            const auto height = std::max(1.0f, available.y);
            rhythm::runtime::ExternalInputs inputs;
            const auto audio_frame = audio_panel.Frame();
            inputs.audio_ = audio_frame.features_;
            observed_audio |= inputs.audio_ && inputs.audio_->valid_ && inputs.audio_->rms_ > 0;
            const auto frame = deck.Tick(smoke ? frames / 60.0 : elapsed, false,
                                         rhythm::player::RenderQuality::kOriginal, renderer, inputs,
                                         audio_frame.playback_, scene_queue);
            if (frame.switched_) {
                beat_panel.Reset();
                control_panel.Reset();
#ifdef RHYTHM_HAS_LOCAL_MEDIA
                if (const auto track = deck.Current().Soundtrack()) {
                    audio_panel.LoadSoundtrack(*track);
                    audio_panel.ApplyPlayback({deck.Current().Paused(), frame.entry_seconds_});
                    if (const auto sample = audio_panel.Frame().playback_) deck.AdoptMedia(*sample);
                    if (smoke) audio_panel.SetVolume(0);
                }
#endif
            }
            const auto& output = frame.output_;
            host.ClearViewerTextures();
            if (output.budget_)
                ImGui::TextWrapped("%s", catalogs.at(chinese ? "zh-CN" : "en-US")
                                                 .at("render.resource_budget")
                                                 .c_str());
            if (renderer.IsValid(output.final_)) {
                const auto fit =
                        rhythm::render::AspectFit(deck.Current().Canvas(), {0, 0, width, height});
                const auto cursor = ImGui::GetCursorPos();
                ImGui::SetCursorPos({cursor.x + fit.x_, cursor.y + fit.y_});
                ImGui::Image(host.RegisterTexture(output.final_), {fit.width_, fit.height_});
            }
            ImGui::End();
            renderer.Submit({}, host.EndUi(), 0x111822ff);
            renderer.EndFrame();
            observed_gpu_output |= renderer.IsValid(output.final_) && renderer.Stats().passes_ >= 2;
            ++frames;
        }
        if (smoke && (frames != 30 || !observed_gpu_output))
            throw std::runtime_error("player.no_gpu_output");
        if (smoke && (requested_audio || deck.Current().Soundtrack()) && !observed_audio)
            throw std::runtime_error("player.no_file_audio_features");
        std::cout << "player_gpu_frames=" << frames << " package_loaded=" << deck.Current().Ready()
                  << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
