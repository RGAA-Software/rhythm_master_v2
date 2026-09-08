#include <bgfx/bgfx.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <chrono>
#include <iostream>

#include "rhythm/assets/store.h"
#include "rhythm/player/session.h"
#include "rhythm/project/store.h"
#include "rhythm/storage/atomic_file.h"
#include "rhythm/studio/studio.h"

namespace {
void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
void Activate(const char* window_name, const char* item) {
    const auto* window = ImGui::FindWindowByName(window_name);
    Check(window != nullptr, "recovery window missing");
    ImGui::ActivateItemByID(ImHashStr(item, 0, window->ID));
}
void PopupAction(const std::string& item, const std::string& child = {}) {
    // Borrowed ImGui windows are checked and confined to this synchronous test adapter.
    const auto& context = *ImGui::GetCurrentContext();
    Check(!context.OpenPopupStack.empty(), "recovery popup missing");
    const auto* popup = context.OpenPopupStack.back().Window;
    Check(popup != nullptr, "recovery popup window missing");
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
    throw std::runtime_error("recovery child window missing");
}
}  // namespace
int main(int argc, char* argv[]) {
    using namespace rhythm;
    try {
        Check(argc == 4, "asset_recovery resources PNG output");
        const auto root =
                std::filesystem::absolute(argv[3]) /
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        const auto project_path = root / "Projects/recovery.rhythmproj";
        const auto package_path = root / "Published/recovery.rhythmpack";
        std::filesystem::create_directories(root);
        const auto original = root / std::filesystem::path(u8"原始素材.png");
        std::filesystem::copy_file(argv[2], original);
        assets::Store store(project_path / "assets");
        const auto asset = store.Import(original, "image/png");
        graph::Registry registry;
        editor::Snapshot snapshot;
        snapshot.document_.id_ = "asset.recovery";
        snapshot.document_.nodes_ = {registry.MakeNode(1, "texture.image"),
                                     registry.MakeNode(2, "output.texture")};
        snapshot.document_.nodes_[0].properties_["asset"] = asset.id_;
        snapshot.document_.edges_ = {{1, 1, 2, "source"}};
        snapshot.document_.output_ = 2;
        snapshot.assets_ = {asset};
        snapshot.title_ = "Asset recovery";
        project::Save(project_path, snapshot);
        const auto blob =
                project_path / "assets/sha256" / asset.id_.sha256_.substr(0, 2) / asset.id_.sha256_;
        storage::WriteDurable(blob, "damaged");
        platform::Host host(true);
        host.Resize({1440, 1000});
        ImGui::GetIO().IniFilename = nullptr;
        auto renderer = host.CreateRenderer();
        auto font = host.CreateFontTexture(renderer);
        studio::Studio studio(argv[1], project_path);
        const auto path_utf8 = original.u8string();
        const std::string input(path_utf8.begin(), path_utf8.end());
        int recovered = -1;
        bool published = false;
        for (int frame = 0; frame < 300 && !published; ++frame) {
            Check(host.Poll(), "recovery host closed");
            host.BeginUi();
            renderer.BeginFrame();
            if (frame == 15) Check(!studio.HasValidPlan(), "damaged asset must not execute");
            if (frame == 20) Activate("###graph", "###asset.manager");
            if (frame == 24) PopupAction("###" + asset.id_.sha256_, "###asset.records");
            if (frame == 27) PopupAction("###asset.source");
            if (frame == 30) ImGui::GetIO().AddInputCharactersUTF8(input.c_str());
            if (frame == 31) ImGui::GetIO().AddKeyEvent(ImGuiKey_Enter, true);
            if (frame == 32) ImGui::GetIO().AddKeyEvent(ImGuiKey_Enter, false);
            if (frame == 35) PopupAction("###asset.restore");
            if (recovered >= 0) {
                if (frame == recovered + 3) ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, true);
                if (frame == recovered + 4) ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, false);
                if (frame == recovered + 8) Activate("###graph", "###save");
                if (frame == recovered + 16) Activate("###graph", "###publish");
            }
            studio.Frame(host, renderer, frame / 60.0);
            if (recovered < 0 && frame > 35 && studio.HasValidPlan() && store.Verify(asset))
                recovered = frame;
            renderer.Submit({}, host.EndUi(), 0x111822ff);
            if (recovered >= 0 && frame == recovered + 1) {
                const auto capture = (root / "repaired-studio").string();
                bgfx::requestScreenShot(BGFX_INVALID_HANDLE, capture.c_str());
            }
            renderer.EndFrame();
            published = std::filesystem::exists(package_path);
        }
        Check(recovered >= 0 && published && studio.Status().authored_nodes_ == 2,
              "open damaged project, UI restore, preview and publish");
        Check(project::Load(project_path).unavailable_assets_.empty(),
              "strict reopen after repair");
        player::Session player;
        player.Open(package_path);
        renderer.BeginFrame();
        const auto output = player.Tick(0, false, {64, 64}, renderer);
        Check(renderer.IsValid(output.final_) && !output.budget_,
              "published repaired image renders");
        auto readback = renderer.RequestReadback(output.final_);
        renderer.EndFrame();
        std::optional<render::ReadbackImage> image;
        for (int frame = 0; frame < 16 && !image; ++frame) {
            image = readback.Poll();
            renderer.BeginFrame();
            renderer.EndFrame();
        }
        Check(image.has_value(), "repaired Player pixels ready");
        bool color = false;
        for (std::size_t offset = 0; offset < image->rgba_.size(); offset += 4)
            color |= image->rgba_[offset] || image->rgba_[offset + 1] || image->rgba_[offset + 2];
        Check(color, "repaired Player nonblack pixels");
        std::cout << "Studio repair workflow and published Player GPU pixels pass: "
                  << root.string() << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
