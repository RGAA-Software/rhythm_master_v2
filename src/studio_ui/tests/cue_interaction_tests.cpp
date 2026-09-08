#include <imgui.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <stdexcept>

#include "cue_editor.h"
#include "rhythm/editor/history.h"

namespace {
struct ContextDeleter {
    void operator()(ImGuiContext* context) const { ImGui::DestroyContext(context); }
};
void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
}  // namespace
int main(int argc, char* argv[]) {
    using namespace rhythm;
    try {
        Check(argc == 2, "locale path");
        std::ifstream input(argv[1]);
        const auto text = nlohmann::json::parse(input).get<std::map<std::string, std::string>>();
        std::unique_ptr<ImGuiContext, ContextDeleter> context(ImGui::CreateContext());
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = {640, 640};
        io.DeltaTime = 1.0f / 60;
        Check(io.Fonts->Build(), "font");
        editor::Snapshot initial;
        initial.document_.id_ = "cue.ui";
        initial.document_.nodes_ = {graph::Registry{}.MakeNode(1, "control.scalar")};
        initial.document_.control_snapshots_ = {{1, "Quiet", {{1, 0}}}, {2, "Peak", {{1, 1}}}};
        initial.document_.control_cues_ = {{1, "Opening", 2, 1, 1}};
        editor::History history(initial);
        std::optional<editor::Snapshot> draft;
        studio::CueEditor editor;
        ImVec2 origin{};
        int commits = 0;
        const auto frame = [&] {
            ImGui::NewFrame();
            ImGui::SetNextWindowPos({0, 0});
            ImGui::SetNextWindowSize({640, 640});
            ImGui::Begin("Cue", nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
            origin = ImGui::GetCursorScreenPos();
            auto edit =
                    editor.Draw((draft ? *draft : history.Current()).document_, 5, 10, 120, text);
            if (edit.cues_) {
                if (!draft) draft = history.Current();
                draft->document_.control_cues_ = *edit.cues_;
            }
            if (edit.committed_ && draft) {
                Check(history.Apply(std::move(*draft), history.Current().document_.revision_),
                      "commit");
                draft.reset();
                ++commits;
            }
            ImGui::End();
            ImGui::Render();
        };
        for (int i = 0; i < 3; ++i) frame();
        const auto row = ImGui::GetFrameHeightWithSpacing();
        const auto drag = [&](ImVec2 from, ImVec2 to) {
            io.AddMousePosEvent(from.x, from.y);
            frame();
            io.AddMouseButtonEvent(0, true);
            frame();
            io.AddMousePosEvent(to.x, to.y);
            frame();
            io.AddMouseButtonEvent(0, false);
            frame();
        };
        const auto y = origin.y + 4 * row + 16;
        drag({origin.x + 140, y}, {origin.x + 202, y});
        Check(commits == 1 && history.Current().document_.control_cues_[0].seconds_ == 3,
              "cue marker drag snaps to beat and commits once");
        Check(history.Undo() && history.Current().document_.control_cues_[0].seconds_ == 2, "undo");
        frame();
        const ImVec2 add{origin.x + 30, origin.y + 3 * row + 9};
        drag(add, add);
        Check(commits == 2 && history.Current().document_.control_cues_.size() == 2 &&
                      history.Current().document_.control_cues_[1].seconds_ == 5,
              "add at playhead");
        // Adding at the same time must not corrupt the existing arrangement.
        drag(add, add);
        Check(commits == 2 && history.Current().document_.control_cues_.size() == 2,
              "duplicate time rejected");
        std::cout << "Cue UI: beat snapping, drag transaction, undo, insertion and invalid edit "
                     "retention passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
