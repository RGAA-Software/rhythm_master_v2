#include <imgui.h>

#include <iostream>
#include <memory>
#include <stdexcept>

#include "rhythm/control_ui/control_panel.h"

namespace {
struct ContextDeleter {
    void operator()(ImGuiContext* context) const { ImGui::DestroyContext(context); }
};
void Check(bool value) {
    if (!value) throw std::runtime_error("control.ui_contract");
}
}  // namespace
int main() {
    using namespace rhythm;
    try {
        std::unique_ptr<ImGuiContext, ContextDeleter> context(ImGui::CreateContext());
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = {640, 640};
        io.DeltaTime = 1.0f / 60;
        Check(io.Fonts->Build());
        parameters::ControlBank bank(
                {{1, "Intensity", 0, 1, 0.2}, {2, "Motion", -1, 1, 0}},
                {{1, "Quiet", {{1, 0}, {2, -1}}}, {2, "Bright", {{1, 1}, {2, 1}}}});
        control_ui::ControlPanel panel;
        auto values = bank.Resolve();
        int commits = 0;
        ImVec2 origin{};
        ImVec2 capture{};
        std::string captured;
        const auto frame = [&] {
            ImGui::NewFrame();
            ImGui::SetNextWindowPos({0, 0});
            ImGui::SetNextWindowSize({640, 640});
            ImGui::Begin("Controls", nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
            origin = ImGui::GetCursorScreenPos();
            auto edit = panel.Draw(bank, values, {}, true);
            if (edit.values_) values = std::move(*edit.values_);
            if (edit.committed_) ++commits;
            if (!edit.capture_.empty()) captured = edit.capture_;
            const auto a = ImGui::GetItemRectMin();
            const auto b = ImGui::GetItemRectMax();
            capture = {(a.x + b.x) / 2, (a.y + b.y) / 2};
            ImGui::End();
            ImGui::Render();
        };
        for (int i = 0; i < 3; ++i) frame();
        const auto drag = [&](ImVec2 first, ImVec2 second) {
            io.AddMousePosEvent(first.x, first.y);
            frame();
            io.AddMouseButtonEvent(0, true);
            frame();
            io.AddMousePosEvent(second.x, second.y);
            frame();
            io.AddMouseButtonEvent(0, false);
            frame();
        };
        const auto row = ImGui::GetFrameHeightWithSpacing();
        drag({origin.x + 40, origin.y + row + 9}, {origin.x + 300, origin.y + row + 9});
        Check(values.at(1) > 0.5 && values.at(2) == 0 && commits == 1);
        const ImVec2 name{origin.x + 80, capture.y - row};
        drag(name, name);
        io.AddInputCharactersUTF8("Stage look");
        frame();
        drag(capture, capture);
        Check(captured == "Stage look" && commits == 1);
        panel.Reset();
        bank = {};
        values.clear();
        frame();
        std::cout << "Control UI: live slider, one gesture commit, named capture and package reset "
                     "passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
