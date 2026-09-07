#include <imgui.h>

#include <iostream>
#include <memory>
#include <stdexcept>

#include "timeline_panel.h"

namespace {
struct ContextDeleter {
    void operator()(ImGuiContext* context) const { ImGui::DestroyContext(context); }
};
void Check(bool value) {
    if (!value) throw std::runtime_error("timeline_ui.contract");
}
}  // namespace
int main() {
    using namespace rhythm;
    try {
        std::unique_ptr<ImGuiContext, ContextDeleter> context(ImGui::CreateContext());
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = {800, 800};
        io.DeltaTime = 1.0f / 60;
        Check(io.Fonts->Build());
        const std::map<std::string, std::string> text{{"timeline.play", "Play"},
                                                      {"timeline.pause", "Pause"},
                                                      {"timeline.restart", "Restart"},
                                                      {"timeline.loop", "Loop"},
                                                      {"timeline.seconds", "Seconds"},
                                                      {"timeline.frames", "Frames"},
                                                      {"timeline.beats", "Beats"},
                                                      {"timeline.duration", "Duration"},
                                                      {"timeline.stateful", "Stateful"},
                                                      {"timeline.media_clock", "Music clock"},
                                                      {"timeline.no_tracks", "No tracks"},
                                                      {"timeline.track", "Track"},
                                                      {"timeline.add_section", "Add section"},
                                                      {"timeline.curve_help", "Curve seconds"},
                                                      {"scalar.curve", "Curve"},
                                                      {"key_time", "Time"},
                                                      {"value", "Value"},
                                                      {"curve.step", "Step"},
                                                      {"curve.linear", "Linear"},
                                                      {"curve.smooth", "Smooth"},
                                                      {"add_key", "Add"},
                                                      {"remove_key", "Remove"},
                                                      {"invalid_curve", "Invalid"}};
        editor::Snapshot initial;
        initial.document_.id_ = "timeline.test";
        initial.document_.nodes_.push_back(graph::Registry{}.MakeNode(1, "scalar.curve"));
        editor::History history(initial);
        studio::TimelinePanel panel;
        ImVec2 pause_center{}, loop_center{}, add_center{};
        int commits = 0;
        const auto frame = [&] {
            ImGui::NewFrame();
            ImGui::SetNextWindowPos({0, 0});
            ImGui::SetNextWindowSize({800, 800});
            ImGui::Begin("Timeline test", nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
            const auto start = ImGui::GetCursorScreenPos();
            const auto& style = ImGui::GetStyle();
            pause_center = {start.x + 10, start.y + ImGui::GetFrameHeight() * 0.5f};
            const auto button = [&](const char* label) {
                return ImGui::CalcTextSize(label).x + style.FramePadding.x * 2 +
                       style.ItemSpacing.x;
            };
            loop_center = {
                    start.x + button(panel.Paused() ? "Play" : "Pause") + button("Restart") + 8,
                    pause_center.y};
            auto edit = panel.Draw(history.Current(), true, text);
            if (edit.committed_) {
                Check(history.Apply(std::move(*edit.committed_),
                                    history.Current().document_.revision_));
                ++commits;
            }
            const auto minimum = ImGui::GetItemRectMin(), maximum = ImGui::GetItemRectMax();
            add_center = {(minimum.x + maximum.x) * 0.5f, (minimum.y + maximum.y) * 0.5f};
            ImGui::End();
            ImGui::Render();
        };
        const auto click = [&](ImVec2 point) {
            io.AddMousePosEvent(point.x, point.y);
            frame();
            io.AddMouseButtonEvent(0, true);
            frame();
            io.AddMouseButtonEvent(0, false);
            frame();
        };
        for (int index = 0; index < 3; ++index) frame();
        Check(panel.Advance(0, true) == 0 && panel.Advance(1, true) == 1);
        click(pause_center);
        Check(panel.Paused() && panel.Advance(5, true) == 1);
        Check(panel.TakePlaybackCommand().paused_ == true);
        click(pause_center);
        Check(!panel.Paused() && panel.Advance(6, true) == 1 && panel.Advance(7, true) == 2);
        click(loop_center);
        Check(panel.Advance(15.25, true) == 0.25 && panel.Advance(15.75, true) == 0.75);
        Check(panel.Generation() == 1);
        click(add_center);
        Check(commits == 1 && std::get<parameters::Curve>(
                                      history.Current().document_.nodes_[0].properties_.at("curve"))
                                              .Keys()
                                              .size() == 3);
        Check(history.Undo() &&
              std::get<parameters::Curve>(
                      history.Current().document_.nodes_[0].properties_.at("curve"))
                              .Keys()
                              .size() == 2);
        panel.TakePlaybackCommand();
        runtime::PlaybackSample music{3, 1, false, 16};
        Check(panel.Advance(16, false, music) == 3);
        frame();
        click(pause_center);
        Check(panel.TakePlaybackCommand().paused_ == true);
        music.paused_ = true;
        Check(panel.Advance(20, false, music) == 3 && panel.Paused());
        panel.Restart();
        Check(panel.TakePlaybackCommand().seek_ == 0);
        music.seconds_ = 0;
        ++music.generation_;
        Check(panel.Advance(21, false, music) == 0);
        const auto generation = panel.Generation();
        Check(panel.Advance(30, false, music) == 0 && panel.Generation() == generation);
        Check(!panel.TakePlaybackCommand().seek_);
        std::cout << "Timeline UI passed: pause/resume, continuous loop and one-command curve "
                     "edit/undo\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
