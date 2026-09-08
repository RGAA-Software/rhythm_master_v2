#include <imgui.h>

#include <iostream>
#include <memory>
#include <stdexcept>

#include "property_inspector.h"
#include "rhythm/graph/controls.h"

namespace {
struct ContextDeleter {
    void operator()(ImGuiContext* context) const { ImGui::DestroyContext(context); }
};
void Check(bool value) {
    if (!value) throw std::runtime_error("control.inspector_contract");
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
        Check(history.Redo() &&
              graph::Scalar(history.Current().document_.nodes_[0], "value", 0) == value);
        inspector.Reset();
        frame();
        std::cout << "Control inspector: preview, one history transaction, undo/redo and reset "
                     "passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
