#include <imgui.h>
#include <imgui_internal.h>

#include <chrono>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <thread>

#include "asset_panel.h"
#include "rhythm/storage/atomic_file.h"

namespace {
struct ContextDelete {
    void operator()(ImGuiContext* context) const { ImGui::DestroyContext(context); }
};
void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
// Checked, synchronous borrowed ImGui windows never escape these UI boundaries.
void Activate(const std::string& item, const std::string& child = {}) {
    const auto& context = *ImGui::GetCurrentContext();
    Check(!context.OpenPopupStack.empty(), "asset popup open");
    const auto* popup = context.OpenPopupStack.back().Window;
    Check(popup != nullptr, "asset popup window");
    if (child.empty()) {
        ImGui::ActivateItemByID(ImHashStr(item.c_str(), 0, popup->ID));
        return;
    }
    for (const auto* window : context.Windows)
        if (window->ParentWindow == popup &&
            std::string_view(window->Name).find(child) != std::string_view::npos) {
            ImGui::ActivateItemByID(ImHashStr(item.c_str(), 0, window->ID));
            return;
        }
    throw std::runtime_error("asset child missing");
}
}  // namespace
int main(int argc, char* argv[]) {
    using namespace rhythm;
    try {
        Check(argc == 3, "asset_ui directory locale");
        const auto root = std::filesystem::absolute(argv[1]);
        std::filesystem::create_directories(root);
        std::ifstream locale(argv[2]);
        const auto text = nlohmann::json::parse(locale).get<std::map<std::string, std::string>>();
        std::unique_ptr<ImGuiContext, ContextDelete> context(ImGui::CreateContext());
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = {1000, 900};
        io.DeltaTime = 1.0f / 60;
        Check(io.Fonts->Build(), "asset UI atlas");
        // Opaque I/O fixtures: this checks authoring gestures, not PNG decoding.
        const auto original = root / "original.png";
        const auto replacement = root / "replacement.png";
        storage::WriteDurable(original, "original fixture");
        storage::WriteDurable(replacement, "replacement fixture");
        assets::Store store(root / "assets");
        const auto record = store.Import(original, "image/png");
        editor::Snapshot snapshot;
        snapshot.document_.id_ = "asset-ui";
        snapshot.assets_ = {record};
        graph::Node node;
        node.id_ = 1;
        node.properties_["asset"] = record.id_;
        snapshot.document_.nodes_ = {node};
        studio::AssetPanel panel;
        studio::AssetEdit completed;
        const auto frame = [&] {
            ImGui::NewFrame();
            ImGui::SetNextWindowPos({0, 0});
            ImGui::SetNextWindowSize({1000, 900});
            ImGui::Begin("Asset UI");
            const auto result = panel.Draw(root / "assets", snapshot, text);
            if (result.added_ || result.removed_ || result.restored_ || result.checked_)
                completed = result;
            ImGui::End();
            ImGui::Render();
        };
        frame();
        {
            const auto* window = ImGui::FindWindowByName("Asset UI");
            Check(window != nullptr, "asset parent window");
            ImGui::ActivateItemByID(ImHashStr("###asset.manager", 0, window->ID));
        }
        frame();
        frame();
        Activate("###" + record.id_.sha256_, "###asset.records");
        frame();
        frame();
        Activate("###asset.remove");
        frame();
        Check(!completed.removed_, "referenced remove disabled");
        Activate("###asset.source");
        frame();
        io.AddInputCharactersUTF8(replacement.string().c_str());
        frame();
        io.AddKeyEvent(ImGuiKey_Enter, true);
        frame();
        io.AddKeyEvent(ImGuiKey_Enter, false);
        frame();
        Activate("###asset.replace");
        frame();
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!completed.added_ && std::chrono::steady_clock::now() < deadline) {
            frame();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        Check(completed.added_ && completed.replaced_ == record.id_,
              "replacement gesture returns typed old/new IDs");
        const auto next = editor::ReplaceAsset(snapshot, *completed.replaced_, *completed.added_);
        Check(std::holds_alternative<editor::Snapshot>(next), "replacement applies");
        Check(store.Read(*completed.added_) == "replacement fixture" &&
                      store.Read(record) == "original fixture",
              "new immutable blob imported and old blob retained for undo");
        snapshot = std::get<editor::Snapshot>(next);
        const auto changed_record = *completed.added_;
        panel.Report("asset.replaced", changed_record.id_);
        completed = {};
        const auto blob = root / "assets/sha256" / changed_record.id_.sha256_.substr(0, 2) /
                          changed_record.id_.sha256_;
        storage::WriteDurable(blob, "corrupt");
        frame();
        Activate("###asset.check");
        frame();
        const auto check_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!completed.checked_ && std::chrono::steady_clock::now() < check_deadline) {
            frame();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        Check(completed.checked_ &&
                      completed.checked_->front().health_ == assets::AssetHealth::kCorrupt,
              "integrity button reports corrupted copy");
        completed = {};
        Activate("###asset.restore");
        frame();
        const auto restore_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!completed.restored_ && std::chrono::steady_clock::now() < restore_deadline) {
            frame();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        Check(completed.restored_ == changed_record.id_ && store.Verify(changed_record),
              "restore button repairs exact original bytes without rewriting graph");
        std::cout << "asset UI: selection, disabled deletion, UTF-8 source input and async "
                     "replacement pass\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
