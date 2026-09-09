#include <imgui.h>
#include <imgui_internal.h>

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

#include "rhythm/platform/host.h"
#include "scene_queue_panel.h"

namespace {
void Activate(const char* window_name, const char* item) {
    // Checked borrowed ImGui pointer stays inside this test UI boundary.
    const auto* window = ImGui::FindWindowByName(window_name);
    if (!window) throw std::runtime_error("scene UI window missing");
    ImGui::ActivateItemByID(ImHashStr(item, 0, window->ID));
}
}  // namespace
// Native arguments are borrowed only for this test entry point.
int main(int argc, char* argv[]) {
    using namespace rhythm;
    try {
        if (argc != 4) throw std::invalid_argument("scene_queue_ui resources first second");
        platform::Host host(true);
        host.Resize({1280, 720});
        ImGui::GetIO().IniFilename = nullptr;
        auto renderer = host.CreateRenderer();
        auto font = host.CreateFontTexture(renderer);
        std::ifstream file(std::filesystem::path(argv[1]) / "locales/en-US/studio.json");
        const auto text = nlohmann::json::parse(file).get<std::map<std::string, std::string>>();
        player::SceneDeck deck;
        deck.Open(argv[2]);
        deck.SetBeatGrid(parameters::BeatSettings{});
        player::SceneQueue queue;
        player_ui::SceneQueuePanel panel;
        const std::vector<player_ui::SceneChoice> choices{{argv[3], {{"en-US", "Next work"}}}};
        bool started = false;
        bool switched = false;
        bool observed_boundary = false;
        for (int frame = 0; frame < 200 && !switched; ++frame) {
            if (!host.Poll()) throw std::runtime_error("scene UI closed");
            queue.Pump(deck.CanPrepareNext());
            host.BeginUi();
            renderer.BeginFrame();
            if (frame == 2) Activate("Harness", "###scene.queue");
            if (frame == 5) Activate("###scene.queue", "###scene.go");
            if (frame == 7) Activate("###scene.queue", "###scene.enqueue");
            if (!started && !queue.Items().empty() && deck.QueueReady(queue.Items().front().id_)) {
                Activate("###scene.queue", "###scene.go");
                started = true;
            }
            ImGui::SetNextWindowPos({0, 0});
            ImGui::SetNextWindowSize({1280, 720});
            ImGui::Begin("Harness");
            if (frame <= 8)
                std::cout << "scene_ui frame=" << frame << " queued=" << queue.Items().size()
                          << " before_draw\n"
                          << std::flush;
            panel.Draw(queue, deck, choices, "en-US", text, parameters::Quantization::kBeat);
            if (frame <= 8) std::cout << "scene_ui after_draw\n" << std::flush;
            ImGui::End();
            if (frame == 5 && deck.Transitioning()) throw std::runtime_error("disabled Go started");
            const auto result = deck.Tick(frame / 60.0, false, player::RenderQuality::kBalanced,
                                          renderer, {}, {}, queue);
            const auto& action = deck.ActionStatus(player::PerformanceActionKind::kNextScene);
            if (!observed_boundary && deck.Transitioning()) {
                if (!action.due_seconds_ || deck.Current().Seconds() < *action.due_seconds_ ||
                    deck.Current().Seconds() - *action.due_seconds_ > 1.0 / 60)
                    throw std::runtime_error("scene UI missed quantized boundary");
                observed_boundary = true;
            }
            if (result.output_.budget_ || deck.Error() != player::SceneTransitionError::kNone)
                throw std::runtime_error("scene UI transition failed");
            switched = result.switched_;
            renderer.Submit({}, host.EndUi(), 0x111822ff);
            renderer.EndFrame();
        }
        if (!started || !switched || !observed_boundary || !queue.Items().empty())
            throw std::runtime_error("scene UI did not switch");
        std::cout << "scene UI: built-in enqueue, disabled Go, ready Go and GPU takeover pass\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
