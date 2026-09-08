#include <imgui.h>
#include <imgui_internal.h>

#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <thread>

#include "component_library_panel.h"

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
        Check(argc == 3, "component_library_ui <library directory> <locale json>");
        const std::filesystem::path directory(argv[1]);
        std::ifstream locale(argv[2]);
        const auto text = nlohmann::json::parse(locale).get<std::map<std::string, std::string>>();
        std::unique_ptr<ImGuiContext, ContextDeleter> context(ImGui::CreateContext());
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = {1000, 900};
        io.DeltaTime = 1.0f / 60;
        Check(io.Fonts->Build(), "font atlas");
        graph::Registry registry;
        graph::ComponentDefinition definition;
        definition.type_ = "component.user.ui";
        definition.title_ = "Library UI fixture";
        definition.nodes_ = {registry.MakeNode(1, "texture.gradient")};
        definition.output_ = 1;
        editor::Snapshot snapshot;
        snapshot.document_.id_ = "library-ui";
        snapshot.document_.components_ = {definition};
        snapshot.document_.nodes_ = {
                registry.MakeNode(10, definition.type_, snapshot.document_.components_)};
        snapshot.document_.output_ = 10;
        studio::ComponentLibraryPanel panel;
        panel.Initialize(directory);
        std::optional<studio::LibraryInsertion> insertion;
        int saves = 0, loads = 0;
        const auto frame = [&](const std::string& activate = {}, const std::string& scope = {}) {
            if (auto result = panel.Take()) insertion = std::move(result);
            ImGui::NewFrame();
            ImGui::SetNextWindowPos({0, 0});
            ImGui::SetNextWindowSize({1000, 900});
            ImGui::Begin("Library UI", nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
            if (!activate.empty()) {
                if (!scope.empty()) ImGui::PushID(scope.c_str());
                ImGui::ActivateItemByID(ImGui::GetID(activate.c_str()));
                if (!scope.empty()) ImGui::PopID();
            }
            const std::array<graph::NodeId, 1> selection{10};
            if (const auto request = panel.Draw(snapshot.document_, selection, text)) {
                if (request->save_instance_)
                    ++saves;
                else
                    ++loads;
                panel.Start(*request, snapshot, directory / "unused-assets", {400, 200});
            }
            ImGui::End();
            ImGui::Render();
        };
        frame();
        frame("###component.user_library");
        frame();
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!saves && std::chrono::steady_clock::now() < deadline) {
            if (!panel.Busy())
                frame("###component.library_save");
            else
                frame();
            frame();
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        Check(saves == 1, "save action did not pass through the visible library panel");
        const auto captured = content::CaptureComponent(snapshot, 10, registry);
        const auto digest = project::Digest(project::EncodeGraph(captured.document_));
        const auto saved = directory / (digest.substr(0, 24) + ".rhythmcomponent");
        const auto scope = saved.filename().string();
        while (!insertion && std::chrono::steady_clock::now() < deadline) {
            if (!loads && !panel.Busy())
                frame(text.at("component.add"), scope);
            else
                frame();
            frame();
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        Check(loads == 1 && insertion && insertion->result_.expected_document_ == "library-ui" &&
                      insertion->position_ == editor::Position{400, 200},
              "saved library entry did not produce one asynchronous insertion");
        Check(std::holds_alternative<editor::Snapshot>(
                      content::InsertComponent(snapshot, *insertion->result_.component_, registry,
                                               insertion->position_, 20)),
              "UI completion cannot be applied as an ordinary editor transaction");
        content::Semantic official;
        official.content_ = snapshot;
        official.content_.document_.components_.front().type_ = "component.official.ui";
        official.content_.document_.nodes_.front().type_ = "component.official.ui";
        official.root_ = official.content_.document_.nodes_.front();
        insertion.reset();
        Check(panel.StartOfficial(official, snapshot, directory / "official-assets", {500, 300}) &&
                      !panel.StartOfficial(official, snapshot, directory / "official-assets", {}),
              "official preparation shares the panel's existing bounded operation");
        const auto official_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!insertion && std::chrono::steady_clock::now() < official_deadline) {
            frame();
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        Check(insertion && insertion->result_.component_ &&
                      insertion->result_.expected_document_ == snapshot.document_.id_ &&
                      insertion->result_.expected_revision_ == snapshot.document_.revision_ &&
                      insertion->position_ == editor::Position{500, 300},
              "official completion preserves revision and the accepted insertion position");
        official.root_.type_ = "invalid.official";
        insertion.reset();
        Check(panel.StartOfficial(official, snapshot, directory / "official-assets", {}),
              "failing official preparation is queued");
        const auto failure_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!insertion && std::chrono::steady_clock::now() < failure_deadline) {
            frame();
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        Check(insertion && !insertion->result_.component_ && !insertion->result_.error_.empty(),
              "failure reaches the main editor status even when the library panel is closed");
        std::cout << "component library UI: open, save, refresh and insert through real ImGui "
                     "activation passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
