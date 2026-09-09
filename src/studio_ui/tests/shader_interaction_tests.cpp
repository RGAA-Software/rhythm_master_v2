#include <imgui.h>
#include <imgui_internal.h>

#include <chrono>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <thread>

#include "rhythm/graph/registry.h"
#include "shader_panel.h"

namespace {
struct ContextDelete {
    void operator()(ImGuiContext* context) const { ImGui::DestroyContext(context); }
};
void Check(bool value, std::string_view message) {
    if (!value) throw std::runtime_error(std::string(message));
}
}  // namespace
int main(int argc, char* argv[]) {
    using namespace rhythm;
    try {
        Check(argc == 6, "shader UI compiler/includes/varying/output/locale");
        std::ifstream file(argv[5]);
        const auto text = nlohmann::json::parse(file).get<std::map<std::string, std::string>>();
        std::unique_ptr<ImGuiContext, ContextDelete> context(ImGui::CreateContext());
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = {900, 700};
        io.DeltaTime = 1.0f / 60;
        Check(io.Fonts->Build(), "font atlas");
        editor::Snapshot initial;
        graph::Registry registry;
        initial.document_.id_ = "shader-ui";
        initial.document_.nodes_ = {registry.MakeNode(1, "texture.shader")};
        editor::History history(initial);
        studio::ShaderPanel panel;
        panel.SetTools({argv[1], argv[2], argv[3]});
        const auto assets = std::filesystem::path(argv[4]) / "assets";
        ImGuiID source_id = 0;
        const auto frame = [&](std::string_view activate = {}) {
            if (auto next = panel.Take(history.Current()))
                history.Apply(std::move(*next), history.Current().document_.revision_);
            ImGui::NewFrame();
            ImGui::SetNextWindowPos({0, 0});
            ImGui::SetNextWindowSize({900, 700});
            ImGui::Begin("Shader UI", nullptr, ImGuiWindowFlags_NoDecoration);
            source_id = ImGui::GetID("###shader.source");
            if (!activate.empty())
                ImGui::ActivateItemByID(ImGui::GetID(std::string(activate).c_str()));
            panel.Draw(history.Current(), 1, assets, text);
            ImGui::End();
            ImGui::Render();
        };
        const auto drain = [&] {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
            while (panel.Busy() && std::chrono::steady_clock::now() < deadline) {
                frame();
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
            Check(!panel.Busy(), "UI compilation deadline");
            frame();
        };
        const auto check_source = [&](std::string_view expected) {
            io.AddMousePosEvent(80, 110);
            frame();
            io.AddMouseButtonEvent(0, true);
            frame();
            io.AddMouseButtonEvent(0, false);
            frame();
            // Checked synchronous borrow of Dear ImGui's active widget state.
            const auto* state = ImGui::GetInputTextState(source_id);
            Check(state && state->TextA.Size > 0, "source widget is focused");
            Check(std::string_view(state->TextA.Data, state->TextLen) == expected,
                  "source widget must contain the restored asset expression");
        };
        frame();
        frame("###shader.compile");
        frame();
        drain();
        Check(history.Current().assets_.size() == 1, "visible compile applies source asset");
        const auto asset = history.Current().assets_[0];

        frame();
        // Exercise text entry, rather than replacing the node property directly.
        io.AddMousePosEvent(80, 110);
        frame();
        io.AddMouseButtonEvent(0, true);
        frame();
        io.AddMouseButtonEvent(0, false);
        frame();
        io.AddKeyEvent(ImGuiMod_Ctrl, true);
        io.AddKeyEvent(ImGuiKey_A, true);
        frame();
        io.AddKeyEvent(ImGuiKey_A, false);
        io.AddKeyEvent(ImGuiMod_Ctrl, false);
        io.AddInputCharactersUTF8("vec4(uv.x, uv.y, sin(time), 1.0)");
        frame();
        frame("###shader.compile");
        frame();
        drain();
        Check(history.Current().assets_.size() == 1 &&
                      history.Current().assets_[0].id_ != asset.id_,
              "edited source replaces the current asset without accumulating unused versions");
        Check(history.Undo(), "undo compiled replacement");
        Check(std::get<assets::AssetId>(
                      history.Current().document_.nodes_[0].properties_.at("asset")) == asset.id_,
              "undo restores the previous successful shader");
        frame();
        drain();
        check_source("vec4(0.5 + 0.5 * cos(time + uv.xyx * 6.283185 + vec3(0.0, 2.0, 4.0)), 1.0)");
        Check(history.Redo(), "redo compiled replacement");
        const auto replacement = history.Current().assets_[0].id_;
        // Undo/redo changes binding; the disconnected node must reload the asset source.
        frame();
        drain();
        check_source("vec4(uv.x, uv.y, sin(time), 1.0)");
        frame("###shader.compile");
        frame();
        drain();
        Check(history.Current().assets_[0].id_ == replacement,
              "disconnected node recompiles its restored source, not the default expression");
        std::cout << "Shader UI: source entry, dual compilation, immutable replacement and "
                     "undo/redo passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
