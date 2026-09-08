#include <imgui.h>
#include <imgui_internal.h>

#include <chrono>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <thread>

#include "rhythm/project/store.h"
#include "soundtrack_panel.h"

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
        Check(argc == 4, "soundtrack_ui music output locale");
        const auto root =
                std::filesystem::path(argv[2]) /
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        std::ifstream catalog(argv[3]);
        const auto text = nlohmann::json::parse(catalog).get<std::map<std::string, std::string>>();
        std::unique_ptr<ImGuiContext, ContextDelete> context(ImGui::CreateContext());
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = {1100, 900};
        io.DeltaTime = 1.0F / 60;
        Check(io.Fonts->Build(), "font atlas");
        editor::Snapshot initial;
        graph::Registry registry;
        initial.document_.id_ = "soundtrack-ui";
        initial.document_.nodes_ = {registry.MakeNode(1, "texture.gradient"),
                                    registry.MakeNode(2, "output.texture")};
        initial.document_.edges_ = {{1, 1, 2, "source"}};
        initial.document_.output_ = 2;
        editor::History history(initial);
        audio_ui::AudioPanel audio;
        audio.SetVolume(0);
        audio.SetLoop(true);
        audio.LoadFile(argv[1]);
        studio::SoundtrackPanel panel;
        const auto assets = root / "work.rhythmproj" / "assets";
        const auto apply = [&](editor::Snapshot next) {
            Check(history.Apply(std::move(next), history.Current().document_.revision_),
                  "history edit");
        };
        const auto frame = [&](const char* activate = nullptr) {
            if (auto edit = panel.Take(history.Current(), false, audio)) apply(std::move(*edit));
            panel.Sync(history.Current(), assets, audio);
            ImGui::NewFrame();
            ImGui::SetNextWindowPos({0, 0});
            ImGui::SetNextWindowSize({1100, 900});
            ImGui::Begin("Soundtrack UI", nullptr, ImGuiWindowFlags_NoDecoration);
            if (activate) ImGui::ActivateItemByID(ImGui::GetID(activate));
            if (const auto action =
                        panel.Draw(history.Current(), audio.SelectedFile().has_value(), text))
                if (auto edit = panel.Start(*action, history.Current(), assets, audio))
                    apply(std::move(*edit));
            ImGui::End();
            ImGui::Render();
        };
        const auto drain = [&] {
            const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            while (panel.Busy() && std::chrono::steady_clock::now() < end) {
                frame();
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
            Check(!panel.Busy(), "music import deadline");
            frame();
        };
        frame();
        frame("###music.bind");
        frame();
        drain();
        const auto bound = history.Current();
        Check(bound.soundtrack_ && bound.soundtrack_->gain_ == 0 && bound.soundtrack_->loop_,
              "visible bind button retains playback settings");
        Check(bound.assets_.size() == 1 && audio.SelectedFile() != std::filesystem::path(argv[1]),
              "binding selects immutable project music");
        audio.SetLoop(false);
        frame("###music.bind");
        frame();
        Check(!history.Current().soundtrack_->loop_ &&
                      history.Current().soundtrack_->title_ == bound.soundtrack_->title_,
              "settings update preserves music title and identity");
        project::Save(root / "work.rhythmproj", history.Current());
        const auto reopened = project::Load(root / "work.rhythmproj").snapshot_;
        Check(reopened.soundtrack_ == history.Current().soundtrack_, "saved UI music settings");
        project::PublishSnapshot(root / "work.rhythmpack", reopened, assets);
        frame("###music.clear");
        frame();
        Check(!history.Current().soundtrack_ && history.Current().assets_.empty(),
              "clear unbinds unused asset");
        Check(history.Undo(), "undo unbind");
        frame();
        Check(history.Current().soundtrack_ && audio.SelectedFile(),
              "undo restores playable immutable music");
        audio.LoadFile(argv[1]);
        frame("###music.bind");
        frame();
        auto changed = history.Current();
        changed.title_ = "edited during import";
        apply(std::move(changed));
        const auto revision = history.Current().document_.revision_;
        drain();
        Check(history.Current().document_.revision_ == revision &&
                      history.Current().title_ == "edited during import",
              "stale import cannot overwrite edits");
        if (std::filesystem::file_size(argv[1]) < 8 * 1024 * 1024) {
            audio.LoadFile(argv[1]);
            frame("###music.append");
            frame();
            drain();
            Check(history.Current().soundtrack_->clips_.size() == 2 && !audio.SelectedFile(),
                  "append converts legacy source and selects arrangement playback");
            project::Save(root / "work.rhythmproj", history.Current());
            project::PublishSnapshot(root / "arranged.rhythmpack", history.Current(), assets);
            frame("###music.load");
            frame();
            Check(!audio.SelectedFile(), "load restores full arrangement, not primary asset");
            frame("###music.clear");
            frame();
            Check(!history.Current().soundtrack_ && history.Current().assets_.empty(),
                  "clear arrangement removes unreferenced audio");
            Check(history.Undo(), "undo arranged clear");
            frame();
            Check(history.Current().soundtrack_->clips_.size() == 2 && !audio.SelectedFile(),
                  "undo restores arranged playback");
        }
        std::cout << "music UI bind/settings/clear/undo/save/publish and stale completion pass\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
