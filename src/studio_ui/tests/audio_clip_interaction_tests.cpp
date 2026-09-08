#include <imgui.h>
#include <imgui_internal.h>

#include <cmath>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

#include "export_panel.h"
#include "time_track_editor.h"

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
        Check(argc == 2, "audio_clip_ui locale");
        std::ifstream catalog(argv[1]);
        const auto text = nlohmann::json::parse(catalog).get<std::map<std::string, std::string>>();
        std::unique_ptr<ImGuiContext, ContextDelete> context(ImGui::CreateContext());
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = {1100, 1100};
        io.DeltaTime = 1.0F / 60;
        Check(io.Fonts->Build(), "font atlas");
        editor::Snapshot initial;
        initial.document_.id_ = "audio-clips-ui";
        initial.assets_ = {{{std::string(64, 'a')}, 1024, "audio/flac"}};
        initial.soundtrack_ =
                media::Soundtrack{{std::string(64, 'a')},
                                  "Music",
                                  1,
                                  false,
                                  {{1, "First", {std::string(64, 'a')}, {2, 4, 0.2, 4.2}}}};
        editor::History history(initial);
        studio::TimeTrackEditor panel;
        ImVec2 origin{};
        float width = 0;
        int commits = 0;
        const auto frame = [&](const char* activate = nullptr) {
            ImGui::NewFrame();
            ImGui::SetNextWindowPos({0, 0});
            ImGui::SetNextWindowSize({1100, 1100});
            ImGui::Begin("Audio clip UI", nullptr, ImGuiWindowFlags_NoDecoration);
            if (activate) ImGui::ActivateItemByID(ImGui::GetID(activate));
            auto edit = panel.Draw(history.Current(), 2, 10, text);
            if (edit.committed_) {
                Check(history.Apply(std::move(*edit.committed_),
                                    history.Current().document_.revision_),
                      "apply gesture");
                ++commits;
            }
            // Borrowed ImGui objects remain inside this synchronous test boundary.
            for (auto* window : ImGui::GetCurrentContext()->Windows)
                if (window->ParentWindow == ImGui::GetCurrentWindow() &&
                    window->ChildId == ImGui::GetID("###audio.clip_rows")) {
                    origin = window->DC.CursorStartPos;
                    width = window->WorkRect.GetWidth();
                }
            ImGui::End();
            ImGui::Render();
        };
        frame();
        frame();
        const auto drag = [&](double from, double to) {
            const int before = commits;
            io.AddMousePosEvent(origin.x + width * static_cast<float>(from / 10), origin.y + 16);
            frame();
            io.AddMouseButtonEvent(0, true);
            frame();
            io.AddMousePosEvent(origin.x + width * static_cast<float>(to / 10), origin.y + 16);
            frame();
            frame();
            Check(commits == before && panel.Preview(), "drag publishes draft, no history entry");
            io.AddMouseButtonEvent(0, false);
            frame();
            frame();
            Check(commits == before + 1 && !panel.Preview(), "release commits one edit");
        };
        Check(width > 0, "visible audio rows");
        drag(4, 5.12);
        const auto moved = history.Current().soundtrack_->clips_.front().timing_;
        Check(moved.start_ == 3 && moved.source_in_ == 0.2 && moved.source_out_ == 4.2,
              "beat-snapped placement retains source trim");
        drag(7, 8);
        Check(history.Current().soundtrack_->clips_.front().timing_.duration_ == 5,
              "right edge changes placement duration");
        Check(history.Undo(), "undo resize");
        panel.Reset();
        frame();
        Check(history.Current().soundtrack_->clips_.front().timing_.duration_ == 4,
              "single undo restores duration");
        for (int index = 0; index < 3; ++index) {
            frame("###audio.clip_duplicate");
            frame();
        }
        Check(history.Current().soundtrack_->clips_.size() == 4, "four overlapping clips accepted");
        const auto revision = history.Current().document_.revision_;
        frame("###audio.clip_duplicate");
        frame();
        Check(history.Current().document_.revision_ == revision && !panel.Preview(),
              "fifth overlap rejected before publishing draft");
        for (int index = 0; index < 4; ++index) {
            frame("###audio.clip_remove");
            frame();
        }
        Check(!history.Current().soundtrack_ && history.Current().assets_.empty(),
              "removing last clip releases its record without resurrecting legacy audio");
        Check(history.Undo(), "undo last clip removal");
        panel.Reset();
        frame();
        Check(history.Current().soundtrack_->clips_.size() == 1 &&
                      history.Current().assets_ == initial.assets_,
              "undo restores last clip and asset record");

        studio::ExportPanel exporting;
        exporting.Open("test.mp4", {}, 1, 0.7F, {640, 360}, true);
        std::optional<studio::ExportRequest> request;
        for (int index = 0; index < 4; ++index) {
            ImGui::NewFrame();
            if (index > 0) {
                if (auto* window = ImGui::FindWindowByName(
                            (text.at("export.open") + "###export.open").c_str()))
                    ImGui::ActivateItemByID(
                            window->GetID((text.at("export.start") + "###export.start").c_str()));
            }
            if (auto next = exporting.Draw(text)) request = std::move(next);
            ImGui::Render();
        }
        Check(request && request->settings_.encoding_.audio_ && !request->settings_.music_,
              "export includes arranged PCM without a file override");
        std::cout
                << "audio clip drag/snap/resize/undo/overlap/removal and arranged export UI pass\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
