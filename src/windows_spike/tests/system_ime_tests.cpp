#include <bgfx/bgfx.h>

#include <chrono>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

#include "rhythm/assets/store.h"
#include "rhythm/project/package.h"
#include "rhythm/project/store.h"
#include "rhythm/studio/studio.h"
#include "studio_input.h"
#include "workflow_evidence.h"

namespace {
void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void Write(const std::filesystem::path& path, const nlohmann::json& value) {
    std::ofstream stream(path);
    stream << value.dump(4) << '\n';
    Check(bool(stream), "IME evidence write failed");
}
}  // namespace
int main(int argc, char** argv) {
    using namespace rhythm;
    try {
        Check(argc == 5, "resources font license output required");
        const std::filesystem::path output(argv[4]);
        const auto path = output / "Projects/ime.rhythmproj";
        assets::Store store(path / "assets");
        const auto font_asset = store.Import(argv[2], "font/otf");
        const auto license = store.Import(argv[3], "text/plain");
        graph::Registry registry;
        editor::Snapshot snapshot;
        snapshot.title_ = "System IME";
        snapshot.document_.id_ = "system-ime";
        snapshot.document_.nodes_ = {registry.MakeNode(1, "texture.text"),
                                     registry.MakeNode(2, "output.texture")};
        snapshot.document_.nodes_[0].properties_["asset"] = font_asset.id_;
        snapshot.document_.nodes_[0].properties_["text_content"] = std::string{};
        snapshot.document_.edges_ = {{1, 1, 2, "source"}};
        snapshot.document_.output_ = 2;
        snapshot.assets_ = {font_asset, license};
        project::Save(path, snapshot);
        platform::Host host(false);
        host.Resize({1400, 900});
        ImGui::GetIO().IniFilename = nullptr;
        auto renderer = host.CreateRenderer();
        auto font = host.CreateFontTexture(renderer);
        studio::Studio studio(argv[1], path);
        testing::WorkflowEvidence evidence(output);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(90);
        int frames = 0;
        const auto frame = [&] {
            Check(host.Poll() && std::chrono::steady_clock::now() < deadline, "IME test timeout");
            host.BeginUi();
            renderer.BeginFrame();
            testing::StudioInput::Place("###graph", {0, 0}, {650, 880});
            testing::StudioInput::Place("###inspector", {660, 0}, {725, 880});
            testing::StudioInput::Place("###output", {20, 440}, {600, 410});
            studio.Frame(host, renderer, frames++ / 60.0);
            renderer.Submit({}, host.EndUi(), 0x111822ff);
            renderer.EndFrame();
            evidence.Record("system-ime", studio);
        };
        testing::StudioInput ui(frame);
        ui.Settle(6);
        while (!studio.HasValidPlan()) frame();
        ui.FindNode(1, "texture.text");
        ui.Focus("###inspector", "###property.1.text_content");
        const auto initial_generation = studio.Workflow().requested_generation_;
        Write(output / "ready.json", {{"generation", initial_generation}});
        bool draft_checked = false;
        while (!std::filesystem::exists(output / "finish.flag")) {
            frame();
            if (!draft_checked && std::filesystem::exists(output / "draft.flag")) {
                Check(studio.Workflow().requested_generation_ == initial_generation,
                      "IME draft committed before Apply/Enter");
                Write(output / "draft.json", {{"generation_unchanged", true}});
                draft_checked = true;
            }
        }
        Check(draft_checked, "system IME draft phase was not exercised");
        while (!studio.HasValidPlan() ||
               studio.Workflow().requested_generation_ == initial_generation)
            frame();
        ui.Button("###graph", "###save");
        editor::Snapshot saved;
        do {
            frame();
            saved = project::Load(path).snapshot_;
        } while (std::get<std::string>(saved.document_.nodes_[0].properties_.at("text_content"))
                         .empty());
        Check(std::get<std::string>(saved.document_.nodes_[0].properties_.at("text_content")) ==
                      "你好",
              "system IME did not commit expected Chinese text");
        ui.Button("###graph", "###publish");
        const auto package_path = output / "Published/ime.rhythmpack";
        while (!std::filesystem::exists(package_path)) frame();
        const auto package = project::LoadPackage(package_path);
        Check(std::get<std::string>(package.program_.instructions_[0].node_.properties_.at(
                      "text_content")) == "你好",
              "publication lost system IME text");
        const auto before_reopen = studio.Workflow().requested_generation_;
        ui.Button("###graph", "###reopen");
        while (!studio.HasValidPlan() || studio.Workflow().requested_generation_ == before_reopen)
            frame();
        bgfx::requestScreenShot(BGFX_INVALID_HANDLE, (output / "reopened").string().c_str());
        ui.Settle(6);
        Write(output / "result.json",
              {{"text", "你好"}, {"draft_unchanged", true}, {"save_publish_reopen", true}});
        std::cout << "System IME draft, committed text and Studio save/publish/reopen passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
