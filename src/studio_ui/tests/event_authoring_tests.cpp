#include <imgui.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <source_location>
#include <stdexcept>
#include <string>

#include "event_authoring.h"
#include "event_track_editor.h"
#include "rhythm/project/package.h"

namespace {
struct ContextDeleter {
    void operator()(ImGuiContext* context) const { ImGui::DestroyContext(context); }
};
void Check(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) throw std::runtime_error("event.authoring:" + std::to_string(location.line()));
}
}  // namespace
int main(int argc, char* argv[]) {
    using namespace rhythm;
    using namespace parameters;
    try {
        Check(argc == 2);
        std::ifstream input(argv[1]);
        const auto text = nlohmann::json::parse(input).get<std::map<std::string, std::string>>();
        std::unique_ptr<ImGuiContext, ContextDeleter> context(ImGui::CreateContext());
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = {700, 800};
        io.DeltaTime = 1.0f / 60;
        Check(io.Fonts->Build());
        graph::Registry registry;
        editor::Snapshot initial;
        initial.document_.id_ = "event-recording-ui";
        initial.document_.nodes_ = {
                registry.MakeNode(1, "event.input"), registry.MakeNode(2, "event.step"),
                registry.MakeNode(3, "texture.gradient"), registry.MakeNode(4, "output.texture")};
        initial.document_.edges_ = {{1, 1, 2, "events"}, {2, 2, 3, "amount"}, {3, 3, 4, "source"}};
        initial.document_.output_ = 4;
        editor::History history(initial);
        studio::EventAuthoring authoring;
        runtime::Runtime runtime;
        auto renderer = render::Renderer::CreateNull();
        runtime::FrameContext frame_context;
        int commits = 0;
        ImVec2 origin{};
        runtime::FrameResult last;
        const auto evaluate = [&] {
            const auto plan = std::get<graph::ExecutionPlan>(
                    graph::Compile(history.Current().document_, registry));
            frame_context.external_.events_ =
                    authoring.Advance(history.Current().document_, plan, frame_context, true);
            renderer.BeginFrame();
            last = runtime.Evaluate(plan, frame_context, renderer);
            renderer.EndFrame();
            authoring.Observe(last);
        };
        const auto ui_frame = [&] {
            frame_context.seconds_ += 1.0 / 60;
            evaluate();
            ImGui::NewFrame();
            ImGui::SetNextWindowPos({0, 0});
            ImGui::SetNextWindowSize({700, 800});
            ImGui::Begin("Actions", nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
            origin = ImGui::GetCursorScreenPos();
            if (auto take = authoring.Draw(history.Current(), 1, text)) {
                Check(history.Apply(std::move(*take), history.Current().document_.revision_));
                ++commits;
            }
            ImGui::End();
            ImGui::Render();
        };
        for (int index = 0; index < 3; ++index) ui_frame();
        const auto click = [&](ImVec2 point, const auto& draw) {
            io.AddMousePosEvent(point.x, point.y);
            draw();
            io.AddMouseButtonEvent(0, true);
            draw();
            io.AddMouseButtonEvent(0, false);
            draw();
            draw();
        };
        const auto row = ImGui::GetFrameHeightWithSpacing();
        click({origin.x + 35, origin.y + 3 * row + 8}, ui_frame);
        Check(authoring.Recording() && commits == 0);
        click({origin.x + 20, origin.y + row + 8}, ui_frame);
        Check(authoring.Captured() == 1 && commits == 0);
        const auto recorded_time = frame_context.seconds_;
        click({origin.x + 35, origin.y + 3 * row + ImGui::GetTextLineHeightWithSpacing() + 8},
              ui_frame);
        Check(!authoring.Recording() && commits == 1);
        const auto saved = std::get<EventTrack>(
                history.Current().document_.nodes_[0].properties_.at("actions"));
        Check(saved.Events().size() == 1 && saved.Events()[0].seconds_ <= recorded_time);
        Check(history.Undo() &&
              std::get<EventTrack>(history.Current().document_.nodes_[0].properties_.at("actions"))
                      .Events()
                      .empty());
        Check(history.Redo());
        // A take is not just a UI count: publication must produce the same step
        // at the recorded media time with no live input provided.
        const auto package = project::DecodePackage(
                project::EncodePackage(history.Current().document_, "Recorded actions"));
        runtime.Reset();
        for (const double seconds :
             {0.0, saved.Events()[0].seconds_ - 0.001, saved.Events()[0].seconds_}) {
            renderer.BeginFrame();
            const auto replay =
                    runtime.Evaluate(package.program_, {seconds, 0, {32, 32}}, renderer);
            renderer.EndFrame();
            for (const auto& output : replay.outputs_)
                if (output.node_ == 2)
                    Check(output.scalar_ == (seconds < saved.Events()[0].seconds_ ? 0 : 1));
        }
        ++frame_context.reset_generation_;
        evaluate();
        Check(authoring.Record(history.Current(), 1));
        Check(authoring.Trigger(1, EventKind::kGate, 1));
        frame_context.advance_state_ = false;
        evaluate();
        Check(authoring.Captured() == 0 && !authoring.Finish(history.Current()));
        Check(!authoring.Trigger(1, EventKind::kPulse, 1));
        frame_context.advance_state_ = true;
        evaluate();
        Check(authoring.Captured() == 1);
        authoring.Cancel();
        Check(!authoring.Recording() &&
              std::get<EventTrack>(
                      history.Current().document_.nodes_[0].properties_.at("actions")) == saved);
        Check(authoring.Record(history.Current(), 1));
        ++frame_context.reset_generation_;
        evaluate();
        Check(!authoring.Recording() && authoring.Status() == "event.take_interrupted");
        Check(authoring.Record(history.Current(), 1));
        auto edited = history.Current();
        edited.document_.nodes_[0].properties_["actions"] = EventTrack{};
        Check(history.Apply(edited, history.Current().document_.revision_));
        evaluate();
        Check(!authoring.Recording() && authoring.Status() == "event.take_target_changed");

        studio::EventTrackEditor track_editor;
        EventTrack track;
        ImVec2 add{};
        int track_commits = 0;
        const auto track_frame = [&] {
            ImGui::NewFrame();
            ImGui::SetNextWindowPos({0, 0});
            ImGui::SetNextWindowSize({700, 800});
            ImGui::Begin("Track", nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
            if (auto next = track_editor.Draw(track, "test", 2.5, text)) {
                track = std::move(*next);
                ++track_commits;
            }
            const auto a = ImGui::GetItemRectMin();
            const auto b = ImGui::GetItemRectMax();
            add = {(a.x + b.x) / 2, (a.y + b.y) / 2};
            ImGui::End();
            ImGui::Render();
            Check(ImGui::GetDrawData()->TotalVtxCount < 50000);
        };
        for (int index = 0; index < 3; ++index) track_frame();
        click(add, track_frame);
        Check(track_commits == 1 && track.Events().size() == 1 &&
              track.Events()[0].seconds_ == 2.5);
        click({50, add.y - 4 * row}, track_frame);
        io.AddKeyEvent(ImGuiMod_Ctrl, true);
        io.AddKeyEvent(ImGuiKey_A, true);
        track_frame();
        io.AddKeyEvent(ImGuiKey_A, false);
        io.AddKeyEvent(ImGuiMod_Ctrl, false);
        io.AddInputCharactersUTF8("3.75");
        track_frame();
        io.AddKeyEvent(ImGuiKey_Enter, true);
        track_frame();
        io.AddKeyEvent(ImGuiKey_Enter, false);
        track_frame();
        Check(track_commits == 1 && track.Events()[0].seconds_ == 2.5);
        click({35, add.y - row}, track_frame);
        Check(track_commits == 2 && track.Events()[0].seconds_ == 3.75);
        const auto remove_x = 8 + ImGui::CalcTextSize(text.at("event.apply_action").c_str()).x +
                              2 * ImGui::GetStyle().FramePadding.x +
                              ImGui::GetStyle().ItemSpacing.x + 25;
        click({remove_x, add.y - row}, track_frame);
        Check(track_commits == 3 && track.Events().empty() && track.LastId() == 1);
        click(add, track_frame);
        Check(track_commits == 4 && track.Events().size() == 1 && track.Events()[0].id_ == 2);
        std::vector<RecordedEvent> full;
        for (std::uint64_t id = 1; id <= 4096; ++id)
            full.push_back({id, double(id), EventKind::kPulse, 1});
        track = EventTrack(std::move(full));
        track_frame();
        click(add, track_frame);
        Check(track_commits == 4 && track.Events().size() == 4096);
        std::cout << "ImGui record/trigger/finish, one undo, published replay, "
                     "pause/cancel/seek/edit and bounded track editor passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
