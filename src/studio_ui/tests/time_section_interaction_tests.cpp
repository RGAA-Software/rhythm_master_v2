#include <imgui.h>
#include <imgui_internal.h>

#include <cmath>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>

#include "rhythm/editor/commands.h"
#include "timeline_panel.h"

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
        Check(argc == 2, "time_section_ui locale");
        std::ifstream input(argv[1]);
        const auto text = nlohmann::json::parse(input).get<std::map<std::string, std::string>>();
        std::unique_ptr<ImGuiContext, ContextDelete> context(ImGui::CreateContext());
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = {1000, 1100};
        io.DeltaTime = 1.0F / 60;
        Check(io.Fonts->Build(), "font atlas");
        graph::Registry registry;
        editor::Snapshot initial;
        initial.document_.id_ = "sections.ui";
        initial.document_.nodes_ = {registry.MakeNode(1, "texture.gradient"),
                                    registry.MakeNode(2, "output.texture")};
        initial.document_.edges_ = {{1, 1, 2, "source"}};
        initial.document_.output_ = 2;
        editor::History history(initial);
        studio::TimelinePanel panel;
        panel.Advance(0, true);
        panel.Advance(2, true);
        ImVec2 origin{};
        float width = 0;
        int commits = 0;
        bool scroll_to_end = false;
        const auto frame = [&](bool add = false) {
            ImGui::NewFrame();
            ImGui::SetNextWindowPos({0, 0});
            ImGui::SetNextWindowSize({1000, 1100});
            ImGui::Begin("Sections test", nullptr, ImGuiWindowFlags_NoDecoration);
            if (add) ImGui::ActivateItemByID(ImGui::GetID("###timeline.add_section"));
            auto edit = panel.Draw(history.Current(), true, text, {},
                                   [&] { return history.ReserveNodeId(); });
            if (edit.committed_) {
                Check(history.Apply(std::move(*edit.committed_),
                                    history.Current().document_.revision_),
                      "commit section edit");
                panel.ResetEdit();
                ++commits;
            }
            // Borrowed Dear ImGui window objects stay inside this synchronous
            // test boundary; only the row's value coordinates are retained.
            for (auto* window : ImGui::GetCurrentContext()->Windows)
                if (window->ParentWindow == ImGui::GetCurrentWindow() &&
                    window->ChildId == ImGui::GetID("###timeline.sections")) {
                    origin = window->DC.CursorStartPos;
                    width = window->WorkRect.GetWidth();
                    if (scroll_to_end) ImGui::SetScrollY(window, window->ScrollMax.y);
                }
            ImGui::End();
            ImGui::Render();
        };
        const auto section = [&]() -> const graph::Node& {
            for (const auto& node : history.Current().document_.nodes_)
                if (node.type_ == "time.envelope") return node;
            throw std::runtime_error("missing section");
        };
        frame();
        frame(true);
        frame();
        frame();
        Check(commits == 1 && graph::Scalar(section(), "clip_start", -1) == 2 && width > 0,
              "add at playhead with generated clock");
        const auto drag = [&](double from, double to) {
            const int before = commits;
            io.AddMousePosEvent(origin.x + width * static_cast<float>(from / 10), origin.y + 16);
            frame();
            io.AddMouseButtonEvent(0, true);
            frame();
            io.AddMousePosEvent(origin.x + width * static_cast<float>(to / 10), origin.y + 16);
            for (int index = 0; index < 4; ++index) frame();
            Check(commits == before && panel.Preview().has_value(),
                  "drag remains a preview transaction");
            io.AddMouseButtonEvent(0, false);
            frame();
            frame();
            Check(commits == before + 1 && !panel.Preview(), "release commits exactly once");
        };
        drag(4, 5);
        // Dear ImGui floors mouse positions to pixels; one pixel defines the
        // achievable time precision of this whole-range interaction.
        const double tolerance = 10.0 / width + 1e-6;
        Check(std::abs(graph::Scalar(section(), "clip_start", -1) - 3) < tolerance,
              "bar moves interval");
        drag(7, 8);
        Check(std::abs(graph::Scalar(section(), "clip_duration", -1) - 5) < tolerance,
              "right handle changes duration");
        drag(3, 2);
        Check(std::abs(graph::Scalar(section(), "clip_start", -1) - 2) < tolerance &&
                      std::abs(graph::Scalar(section(), "clip_duration", -1) - 6) < tolerance,
              "left handle preserves end");
        Check(history.Undo(), "undo left trim");
        panel.ResetEdit();
        frame();
        Check(std::abs(graph::Scalar(section(), "clip_start", -1) - 3) < tolerance,
              "one undo restores trim");
        Check(history.Undo(), "undo right trim");
        panel.ResetEdit();
        frame();
        Check(std::abs(graph::Scalar(section(), "clip_duration", -1) - 4) < tolerance,
              "one undo restores duration");
        Check(history.Undo(), "undo movement");
        panel.ResetEdit();
        frame();
        Check(graph::Scalar(section(), "clip_start", -1) == 2, "one undo restores position");
        Check(history.Undo() && history.Current().document_.nodes_ == initial.document_.nodes_,
              "one undo removes generated section and clock");
        panel.ResetEdit();
        auto many = history.Current();
        for (int index = 0; index < 12; ++index) {
            const auto id = history.ReserveNodeId();
            const auto clock = history.ReserveNodeId();
            many = std::get<editor::Snapshot>(editor::AddTimeSection(many, registry, 1, id, clock));
        }
        Check(history.Apply(std::move(many), history.Current().document_.revision_),
              "many section rows");
        frame();
        frame();
        const auto before = commits;
        io.AddMousePosEvent(origin.x + width * 0.3F, origin.y + 16);
        frame();
        io.AddMouseButtonEvent(0, true);
        frame();
        io.AddMousePosEvent(origin.x + width * 0.4F, origin.y + 16);
        frame();
        Check(panel.Preview().has_value(), "scrolled gesture starts as preview");
        scroll_to_end = true;
        frame();
        frame();
        frame();
        io.AddMouseButtonEvent(0, false);
        frame();
        Check(commits == before + 1 && !panel.Preview(),
              "offscreen row release closes transaction");
        scroll_to_end = false;
        auto clips = initial;
        clips.document_.nodes_[0] = registry.MakeNode(1, "texture.video_clip");
        clips.document_.nodes_[0].properties_["clip_start"] = 2.0;
        clips.document_.nodes_[0].properties_["clip_duration"] = 4.0;
        clips.document_.nodes_[0].properties_["source_in"] = 0.2;
        clips.document_.nodes_[0].properties_["source_out"] = 0.8;
        Check(history.Apply(clips, history.Current().document_.revision_), "replace with clip");
        panel.ResetEdit();
        frame();
        frame();
        drag(4, 5);
        const auto& moved = history.Current().document_.nodes_[0];
        Check(std::abs(graph::Scalar(moved, "clip_start", -1) - 3) < tolerance &&
                      graph::Scalar(moved, "source_in", -1) == 0.2 &&
                      graph::Scalar(moved, "source_out", -1) == 0.8,
              "clip placement drag preserves source trim");
        Check(history.Undo(), "one undo restores clip placement");
        panel.ResetEdit();
        frame();
        Check(graph::Scalar(history.Current().document_.nodes_[0], "clip_start", -1) == 2,
              "clip undo restores original start");
        std::cout << "time section and video clip UI add/move/trim and one-command undo pass\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
