#include <imgui.h>
#include <imgui_internal.h>

#include <iostream>
#include <memory>
#include <source_location>
#include <stdexcept>
#include <string>

#include "beat_performance.h"
#include "property_inspector.h"
#include "rhythm/graph/controls.h"

namespace {
struct ContextDeleter {
    void operator()(ImGuiContext* context) const { ImGui::DestroyContext(context); }
};
void Check(bool value, std::source_location location = std::source_location::current()) {
    if (!value)
        throw std::runtime_error("control.inspector_contract:" + std::to_string(location.line()));
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
        graph::Registry registry;
        editor::Snapshot initial;
        initial.document_.id_ = "control.inspector";
        initial.document_.nodes_ = {registry.MakeNode(1, "control.scalar")};
        initial.document_.nodes_[0].properties_["value"] = 0.2;
        editor::History history(initial);
        studio::PropertyInspector inspector;
        int commits = 0;
        bool previewed = false;
        ImVec2 origin{};
        const auto frame = [&] {
            ImGui::NewFrame();
            ImGui::SetNextWindowPos({0, 0});
            ImGui::SetNextWindowSize({640, 640});
            ImGui::Begin("Inspector", nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
            origin = ImGui::GetCursorScreenPos();
            auto edit = inspector.Draw(history.Current(), 0, registry, {}, {}, "en-US");
            Check(!edit.diagnostic_);
            if (edit.preview_changed_ && inspector.Preview()) previewed = true;
            if (edit.committed_) {
                Check(history.Apply(std::move(*edit.committed_),
                                    history.Current().document_.revision_));
                ++commits;
            }
            ImGui::End();
            ImGui::Render();
        };
        for (int i = 0; i < 3; ++i) frame();
        const auto y = origin.y + ImGui::GetFrameHeightWithSpacing() + 9;
        io.AddMousePosEvent(origin.x + 40, y);
        frame();
        io.AddMouseButtonEvent(0, true);
        frame();
        io.AddMousePosEvent(origin.x + 300, y);
        frame();
        Check(previewed && commits == 0 && inspector.Preview().has_value());
        io.AddMouseButtonEvent(0, false);
        frame();
        Check(commits == 1 && !inspector.Preview());
        const auto value = graph::Scalar(history.Current().document_.nodes_[0], "value", 0);
        Check(value > 0.5);
        Check(inspector.LiveControls(graph::DescribeControls(history.Current().document_)).at(1) ==
              value);
        Check(history.Undo() &&
              graph::Scalar(history.Current().document_.nodes_[0], "value", 0) == 0.2);
        frame();
        Check(inspector.LiveControls(graph::DescribeControls(history.Current().document_)).empty());
        {
            auto musical = history.Current();
            const auto saved_value = graph::Scalar(musical.document_.nodes_[0], "value", 0);
            musical.document_.beat_grid_ = parameters::BeatSettings{};
            musical.document_.control_snapshots_ = {{1, "Quiet", {{1, 0.1}}}};
            const auto bank = graph::DescribeControls(musical.document_);
            studio::BeatPerformance performance;
            double seconds = 0.1;
            inspector.PerformControls({{1, 0.9}});
            const auto performance_frame = [&](const char* activate = nullptr, bool popup = false,
                                               bool controls = false) {
                ImGui::NewFrame();
                if (activate) {
                    // Checked ImGui borrows stay in this synchronous test boundary.
                    const auto* window =
                            popup ? ImGui::GetCurrentContext()->OpenPopupStack.back().Window
                                  : ImGui::FindWindowByName("Performance");
                    Check(window != nullptr);
                    const auto scope =
                            controls ? ImHashStr("public_controls", 0, window->ID) : window->ID;
                    ImGui::ActivateItemByID(ImHashStr(activate, 0, scope));
                }
                if (auto values = performance.Advance({seconds, 1, false}, 1,
                                                      musical.document_.beat_grid_, bank, true))
                    inspector.PerformControls(std::move(*values));
                ImGui::SetNextWindowPos({0, 0});
                ImGui::SetNextWindowSize({640, 640});
                ImGui::Begin("Performance", nullptr,
                             ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
                Check(!performance.Draw(musical, seconds, {}));
                const auto edit =
                        inspector.Draw(musical, 0, registry, {}, {}, "en-US", {}, seconds, true);
                Check(!edit.committed_ && !edit.preview_changed_ && !edit.diagnostic_);
                if (edit.recall_) performance.RequestSnapshot(*edit.recall_);
                ImGui::End();
                ImGui::Render();
            };
            performance_frame();
            performance_frame();
            performance_frame("###beat.title");
            performance_frame();
            performance_frame("###beat.quantization");
            performance_frame();
            Check(!ImGui::GetCurrentContext()->OpenPopupStack.empty());
            performance_frame("beat.next_beat", true);
            performance_frame();
            performance_frame("###controls.recall", false, true);
            performance_frame();
            Check(performance.Status().due_seconds_ == 0.5);
            Check(inspector.LiveControls(bank).at(1) == 0.9);
            seconds = 0.49;
            performance_frame();
            Check(inspector.LiveControls(bank).at(1) == 0.9);
            seconds = 0.5;
            performance_frame();
            Check(inspector.LiveControls(bank).at(1) == 0.1 &&
                  performance.Status().state_ == player::PerformanceActionState::kCompleted);
            Check(graph::Scalar(musical.document_.nodes_[0], "value", 0) == saved_value);
        }
        Check(history.Redo() &&
              graph::Scalar(history.Current().document_.nodes_[0], "value", 0) == value);
        inspector.PerformControls({{1, 0.9}});
        frame();
        frame();
        Check(inspector.LiveControls(graph::DescribeControls(history.Current().document_)).at(1) ==
              0.9);
        Check(graph::Scalar(history.Current().document_.nodes_[0], "value", 0) == value &&
              commits == 1);
        inspector.Reset();
        frame();
        Check(inspector.LiveControls(graph::DescribeControls(history.Current().document_)).empty());
        std::cout << "Control inspector: preview, one history transaction, undo/redo and reset "
                     "passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
