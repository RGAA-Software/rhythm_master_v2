#include <imgui.h>
#include <imgui_internal.h>

#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <thread>

#include "waveform_panel.h"

namespace {
struct ContextDelete {
    void operator()(ImGuiContext* context) const { ImGui::DestroyContext(context); }
};
void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
}  // namespace
int main(int argc, char* argv[]) {
    using namespace rhythm;
    try {
        Check(argc == 3, "waveform_ui music locale");
        std::ifstream input(argv[2]);
        const auto text = nlohmann::json::parse(input).get<std::map<std::string, std::string>>();
        std::unique_ptr<ImGuiContext, ContextDelete> context(ImGui::CreateContext());
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = {900, 400};
        io.DeltaTime = 1.0F / 60;
        Check(io.Fonts->Build(), "font atlas");
        studio::WaveformPanel panel;
        std::optional<std::filesystem::path> source = argv[1];
        ImVec2 minimum{}, maximum{};
        bool ready = false, disabled = false;
        int commits = 0;
        double position = 0;
        const auto frame = [&] {
            ImGui::NewFrame();
            ImGui::SetNextWindowPos({0, 0});
            ImGui::SetNextWindowSize({900, 400});
            ImGui::Begin("Waveform test", nullptr, ImGuiWindowFlags_NoDecoration);
            const auto id = ImGui::GetID("###waveform.seek");
            ImGui::BeginDisabled(disabled);
            const auto seek = panel.Draw(source, position, text);
            ready = ImGui::GetItemID() == id;
            if (ready) {
                minimum = ImGui::GetItemRectMin();
                maximum = ImGui::GetItemRectMax();
            }
            if (seek) {
                ++commits;
                position = *seek;
            }
            ImGui::EndDisabled();
            ImGui::End();
            ImGui::Render();
        };
        const auto wait_ready = [&] {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            do {
                frame();
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            } while (!ready && std::chrono::steady_clock::now() < deadline);
            Check(ready, "background overview appears");
        };
        wait_ready();
        const auto point = [&](float fraction) {
            io.AddMousePosEvent(minimum.x + (maximum.x - minimum.x) * fraction, minimum.y + 40);
        };
        point(0.25F);
        frame();
        io.AddMouseButtonEvent(0, true);
        frame();
        Check(commits == 0, "press does not seek decoder");
        for (int index = 0; index < 8; ++index) {
            point(0.75F);
            frame();
        }
        Check(commits == 0, "drag previews position without repeated decoder seeks");
        io.AddMouseButtonEvent(0, false);
        frame();
        Check(commits == 1 && std::abs(position - 96) < 0.001, "one exact seek on release");
        frame();
        Check(commits == 1, "seek is not repeated next frame");
        disabled = true;
        point(0.5F);
        frame();
        io.AddMouseButtonEvent(0, true);
        frame();
        io.AddMouseButtonEvent(0, false);
        frame();
        Check(commits == 1, "disabled inspector transaction prevents waveform edits");
        disabled = false;
        source = std::filesystem::path(argv[1]).parent_path() / "missing.music";
        frame();
        Check(!ready, "source switch immediately hides previous waveform");
        source = argv[1];
        wait_ready();
        source.reset();
        frame();
        Check(!ready, "cleared source hides waveform");
        source = argv[1];
        frame();
        panel.Clear();
        frame();
        wait_ready();
        std::cout << "waveform UI: async replacement, stale cancellation, one seek on release, "
                     "disabled edits and clear pass\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
