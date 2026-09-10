#include <bgfx/bgfx.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#include "rhythm/audio/playback.h"
#include "rhythm/player/session.h"
#include "rhythm/project/store.h"
#include "rhythm/studio/studio.h"

// Short diagnostic, not a soak or FPS acceptance assertion. Run serially through
// verify_windows.py. Keep raw per-frame measurements, including slow frames.
int main(int argc, char* argv[]) {
    using namespace rhythm;
    using Clock = std::chrono::steady_clock;
    try {
        if (argc != 6) throw std::invalid_argument("resources template package output mode");
        const std::string mode = argv[5];
        if (mode != "player" && mode != "studio" && mode != "studio-no-previews" &&
            mode != "studio-detail" && mode != "studio-focus")
            throw std::invalid_argument("performance mode");
        const std::filesystem::path output(argv[4]);
        std::filesystem::create_directories(output);
        platform::Host host(true);
        host.Resize({1280, 720});
        ImGui::GetIO().IniFilename = nullptr;
        auto renderer = host.CreateRenderer();
        auto font = host.CreateFontTexture(renderer);
        player::Session session;
        audio::FilePlayback playback;
        std::unique_ptr<studio::Studio> editor;
        graph::NodeId focus_node = 0;
        if (mode == "player") {
            session.Open(argv[3]);
            const auto soundtrack = session.Soundtrack();
            if (!soundtrack) throw std::runtime_error("missing soundtrack");
            playback.SetVolume(0);
            playback.LoadSoundtrack(*soundtrack);
        } else {
            const auto project_path = output / "probe.rhythmproj";
            const auto prepared = project::PrepareTemplate(argv[2], project_path / "assets");
            for (const auto& node : prepared.snapshot_.document_.nodes_)
                if (node.type_ == "scene.render") focus_node = node.id_;
            project::Save(project_path, prepared.snapshot_);
            editor = std::make_unique<studio::Studio>(argv[1], project_path);
        }
        std::ofstream csv(output / "frames.csv");
        csv << "frame,work_ms,present_ms,total_ms,draws,passes,texture_bytes,inline_previews,audio_"
               "rms,gpu_ms,render_cpu_ms,wait_submit_ms,application_ms,ui_translate_ms,ui_submit_"
               "ms,ui_vertices,ui_indices\n";
        const int warmup = mode == "studio-detail" || mode == "studio-focus" ? 240 : 120;
        const auto start = Clock::now();
        for (int frame = 0; frame < warmup + 480; ++frame) {
            if (!host.Poll()) throw std::runtime_error("probe host closed");
            const auto begin = Clock::now();
            const auto seconds = std::chrono::duration<double>(begin - start).count();
            if (mode == "studio-detail" && frame >= 60 && frame < 80) {
                if (frame < 72)
                    ImGui::GetIO().AddMousePosEvent(680, 267);
                else
                    ImGui::GetIO().AddMousePosEvent(570, 347);
                ImGui::GetIO().AddMouseWheelEvent(0, 1);
            }
            if (mode == "studio-detail" && frame >= 85 && frame <= 101) {
                ImGui::GetIO().AddMousePosEvent(500, 300 + float(std::min(frame - 85, 15)) * 20);
                if (frame == 85) ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Right, true);
                if (frame == 101) ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Right, false);
            }
            host.BeginUi();
            renderer.BeginFrame();
            std::size_t previews = 0;
            float rms = 0;
            if (editor) {
                if (mode == "studio-focus") {
                    if (!focus_node) throw std::runtime_error("no scene.render to focus");
                    if (frame == 60) {
                        const auto* window = ImGui::FindWindowByName("###graph");
                        if (!window) throw std::runtime_error("missing graph window");
                        ImGui::ActivateItemByID(ImHashStr("###navigation.find", 0, window->ID));
                    }
                    if (frame == 65) ImGui::GetIO().AddInputCharactersUTF8("scene.render");
                    if (frame == 75) {
                        bool found = false;
                        for (const auto* window : ImGui::GetCurrentContext()->Windows) {
                            if (!window->WasActive ||
                                std::string_view(window->Name).find("navigation.rows") ==
                                        std::string_view::npos)
                                continue;
                            const auto label = "###node." + std::to_string(focus_node);
                            ImGui::ActivateItemByID(ImHashStr(label.c_str(), 0, window->ID));
                            found = true;
                        }
                        if (!found) throw std::runtime_error("node search results missing");
                    }
                    if (frame >= warmup && editor->Workflow().selected_author_node_ != focus_node)
                        throw std::runtime_error(
                                "node search/focus did not select current render node");
                }
                if (frame == 60 && mode == "studio-no-previews") {
                    // Borrowed upstream context is used only in this UI boundary.
                    const auto* window = ImGui::FindWindowByName("###graph");
                    if (!window) throw std::runtime_error("graph window missing");
                    ImGui::ActivateItemByID(ImHashStr("###viewers", 0, window->ID));
                }
                editor->Frame(host, renderer, seconds);
                const auto status = editor->Status();
                previews = status.inline_previews_;
                rms = status.audio_rms_;
                if (frame >= warmup && (!editor->HasValidPlan() || status.budget_limited_))
                    throw std::runtime_error("probe current plan/output invalid");
                if (frame >= warmup && mode == "studio-no-previews" && previews)
                    throw std::runtime_error("preview toggle failed");
            } else {
                const auto state = playback.Snapshot();
                runtime::ExternalInputs inputs;
                inputs.audio_ = state.features_;
                if (state.features_) rms = state.features_->rms_;
                const runtime::PlaybackSample sample{
                        state.position_seconds_, state.generation_,
                        state.paused_ || state.state_ != audio::PlaybackState::kPlaying,
                        state.duration_seconds_};
                const auto result =
                        session.Tick(seconds, false, {1280, 720}, renderer, inputs, sample);
                if (result.budget_ || !renderer.IsValid(result.final_))
                    throw std::runtime_error("probe player output invalid");
                render::DrawList draw;
                draw.width_ = 1280;
                draw.height_ = 720;
                draw.vertices_ = {{0, 0, 0, 0}, {1280, 0, 1, 0}, {1280, 720, 1, 1}, {0, 720, 0, 1}};
                draw.indices_ = {0, 1, 2, 0, 2, 3};
                draw.commands_ = {{result.final_, 0, 6, {0, 0, 1280, 720}}};
                renderer.Submit({}, draw);
            }
            const auto application_end = Clock::now();
            const auto ui = host.EndUi();
            const auto translation_end = Clock::now();
            renderer.Submit({}, ui);
            if (frame == warmup + 180) {
                const auto capture = (output / "output").string();
                bgfx::requestScreenShot(BGFX_INVALID_HANDLE, capture.c_str());
            }
            const auto submitted = Clock::now();
            renderer.EndFrame();
            const auto end = Clock::now();
            const auto ms = [](auto duration) {
                return std::chrono::duration<double, std::milli>(duration).count();
            };
            const auto stats = renderer.Stats();
            csv << frame << ',' << ms(submitted - begin) << ',' << ms(end - submitted) << ','
                << ms(end - begin) << ',' << stats.draws_ << ',' << stats.passes_ << ','
                << stats.texture_bytes_ << ',' << previews << ',' << rms;
            // Borrowed backend statistics never leave this diagnostic boundary.
            if (const auto* timing = bgfx::getStats()) {
                const auto ticks = [](auto begin, auto end, auto frequency) {
                    return frequency > 0 ? 1000.0 * double(end - begin) / double(frequency) : -1.0;
                };
                csv << ',' << ticks(timing->gpuTimeBegin, timing->gpuTimeEnd, timing->gpuTimerFreq)
                    << ',' << ticks(timing->cpuTimeBegin, timing->cpuTimeEnd, timing->cpuTimerFreq)
                    << ',' << ticks(0, timing->waitSubmit, timing->cpuTimerFreq);
            } else {
                csv << ",-1,-1,-1";
            }
            csv << ',' << ms(application_end - begin) << ','
                << ms(translation_end - application_end) << ',' << ms(submitted - translation_end)
                << ',' << ui.vertices_.size() << ',' << ui.indices_.size() << '\n';
        }
        std::cout << mode << ": " << warmup << " warmup + 480 measured frames; " << output << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
