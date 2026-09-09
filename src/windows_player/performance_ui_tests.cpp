#include <imgui.h>
#include <imgui_internal.h>

#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <thread>

#include "performance_panel.h"
#include "rhythm/project/performance_store.h"
#include "rhythm/storage/atomic_file.h"

namespace {
struct ContextDeleter {
    void operator()(ImGuiContext* context) const { ImGui::DestroyContext(context); }
};
void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
void Activate(const char* window_name, const std::string& item) {
    // Checked ImGui borrowing is restricted to this synchronous test boundary.
    const auto* window = ImGui::FindWindowByName(window_name);
    Check(window != nullptr, "performance window missing");
    ImGui::ActivateItemByID(ImHashStr(item.c_str(), 0, window->ID));
}
}  // namespace
int main(int argc, char** argv) {
    using namespace rhythm;
    try {
        Check(argc == 4, "locale, language and output directory required");
        std::ifstream input(argv[1]);
        const auto text = nlohmann::json::parse(input).get<std::map<std::string, std::string>>();
        const std::string locale(argv[2]);
        const auto directory =
                std::filesystem::path(argv[3]) /
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        std::filesystem::create_directories(directory);
        graph::Registry registry;
        graph::Document document;
        document.id_ = "performance-ui";
        document.nodes_ = {registry.MakeNode(1, "texture.gradient"),
                           registry.MakeNode(2, "output.texture")};
        document.edges_ = {{1, 1, 2, "source"}};
        document.output_ = 2;
        const auto source = directory / "source.rhythmpack";
        storage::WriteDurable(source, project::EncodePackage(document, "UI program"));
        auto work = player::WorkLibrary(directory / "seed").Import(source).reference_;
        work.source_ = performance::WorkSource::kBuiltin;
        work.content_id_ = "official.templates.ui";
        work.version_ = "0.1.0";
        work.policy_ = performance::VersionPolicy::kCurrentBuiltin;
        player::PerformanceProgram program(
                directory / "program", {work},
                [source](const performance::WorkReference&, std::stop_token) {
                    return storage::FileBytes::Open(source, project::kMaximumFilePackageBytes);
                });
        player::SceneQueue queue;
        player_ui::PerformancePanel panel;
        const std::vector<player_ui::SceneChoice> choices{{source, {{locale, "Work"}}, work}};
        std::unique_ptr<ImGuiContext, ContextDeleter> context(ImGui::CreateContext());
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = {1000, 900};
        io.DeltaTime = 1.0f / 60;
        Check(io.Fonts->Build(), "font build failed");
        const auto frame = [&] {
            program.Pump();
            if (auto resolved = program.TakeResolved())
                Check(queue.ReplacePerformance(*resolved), "UI resolution not installable");
            ImGui::NewFrame();
            ImGui::SetNextWindowPos({0, 0});
            ImGui::SetNextWindowSize({1000, 900});
            ImGui::Begin("Harness");
            panel.Draw(program, choices, locale, text);
            ImGui::End();
            ImGui::Render();
        };
        for (int index = 0; index < 3; ++index) frame();
        Activate("Harness", "###performance.open");
        frame();
        frame();
        const auto click = [&](const std::string& id) {
            Activate("###performance.open", "###performance." + id);
            frame();
            frame();
        };
        const auto wait = [&] {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            while (program.Busy() && std::chrono::steady_clock::now() < deadline) {
                frame();
                std::this_thread::yield();
            }
            Check(!program.Busy() && program.Status().error_.empty(), "UI job failed");
        };
        click("add");
        Check(program.Draft().Entries().size() == 1, "Add did not change program");
        click("duplicate");
        Check(program.Draft().Entries().size() == 2 && program.Draft().Entries()[1].id_ == 2,
              "duplicate lost identity");
        click("up");
        Check(program.Draft().Entries()[0].id_ == 2, "UI reorder failed");
        click("save");
        wait();
        const auto saved = project::LoadPerformanceList(directory / "program");
        Check(saved == program.Draft(), "UI save differs from draft");
        click("remove");
        Check(program.Draft().Entries().size() == 1 && program.Status().dirty_, "remove failed");
        click("reopen");
        wait();
        Check(program.Draft() == saved, "UI reopen did not restore order and duplicate");
        click("prepare");
        wait();
        Check(queue.Items().size() == 2 && queue.Items()[0].entry_->id_ == 2 &&
                      queue.Items()[1].entry_->id_ == 1 && program.Draft() == saved,
              "UI preparation changed program or queue order");
        const auto first = queue.Items()[0].id_;
        Check(queue.Remove(first) && program.Draft() == saved, "playback consumed saved program");
        std::cout << "ImGui performance add/duplicate/reorder/save/remove/reopen/prepare passed "
                  << locale << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
