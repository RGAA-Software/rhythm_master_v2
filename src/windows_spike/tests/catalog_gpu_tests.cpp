#include <bgfx/bgfx.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string_view>

#include "template_browser.h"

namespace {
ImVec2 ChildPoint(std::string_view name, bool last_item) {
    // Borrow ImGui windows only during this synchronous test-adapter query.
    for (const auto* window : ImGui::GetCurrentContext()->Windows)
        if (std::string_view(window->Name).find(name) != std::string_view::npos)
            return {window->DC.CursorStartPos.x + 45,
                    (last_item ? window->DC.CursorPosPrevLine.y : window->DC.CursorStartPos.y) +
                            25};
    throw std::runtime_error("catalog.child_missing");
}
ImVec2 CatalogSize() {
    for (const auto* window : ImGui::GetCurrentContext()->Windows)
        if (std::string_view(window->Name).find("catalog.entries") != std::string_view::npos &&
            window->ParentWindow)
            return window->ParentWindow->Size;
    throw std::runtime_error("catalog.popup_missing");
}
ImVec2 CatalogSearchPoint() {
    for (const auto* window : ImGui::GetCurrentContext()->Windows)
        if (std::string_view(window->Name).find("catalog.entries") != std::string_view::npos &&
            window->ParentWindow)
            return {window->ParentWindow->Pos.x + 25, window->ParentWindow->Pos.y + 20};
    throw std::runtime_error("catalog.popup_missing");
}
bool EmptyCatalog() {
    for (const auto* window : ImGui::GetCurrentContext()->Windows)
        if (std::string_view(window->Name).find("catalog.entries") != std::string_view::npos)
            return window->ContentSize.y < 80;
    return false;
}
}  // namespace
int main(int argc, char* argv[]) {
    using namespace rhythm;
    try {
        if (argc != 3) throw std::invalid_argument("catalog.arguments");
        const std::filesystem::path resources(argv[1]), output(argv[2]);
        auto entries = project::ScanTemplates(resources / "content/templates");
        const auto animated = std::find_if(entries.begin(), entries.end(), [](const auto& entry) {
            return entry.id_ == "official.templates.aurora_clouds";
        });
        if (animated == entries.end()) throw std::runtime_error("catalog.animated_reference");
        std::rotate(entries.begin(), animated, std::next(animated));
        std::ifstream file(resources / "locales/zh-CN/studio.json");
        const auto text = nlohmann::json::parse(file).get<std::map<std::string, std::string>>();
        platform::Host host(true);
        auto renderer = host.CreateRenderer();
        auto font = host.CreateFontTexture(renderer);
        studio::TemplateBrowser browser;
        bool observed_preview = false;
        bool observed_empty = false;
        std::optional<std::size_t> applied;
        std::optional<ImVec2> popup_size;
        for (int frame = 0; frame < 300; ++frame) {
            if (!host.Poll()) throw std::runtime_error("catalog.closed");
            auto& io = ImGui::GetIO();
            if (frame == 7 || frame == 8 || frame == 9) {
                const auto point = ChildPoint("catalog.entries", false);
                io.AddMousePosEvent(point.x, point.y);
                if (frame != 7) io.AddMouseButtonEvent(ImGuiMouseButton_Left, frame == 8);
            }
            if (frame >= 150 && frame <= 153) {
                const auto point = CatalogSearchPoint();
                io.AddMousePosEvent(point.x, point.y);
                if (frame == 152 || frame == 153)
                    io.AddMouseButtonEvent(ImGuiMouseButton_Left, frame == 152);
            }
            if (frame == 155) io.AddInputCharactersUTF8("no-template-matches-this-query");
            if (frame == 240 || frame == 241) io.AddKeyEvent(ImGuiKey_Escape, frame == 240);
            host.BeginUi();
            renderer.BeginFrame();
            host.ClearViewerTextures();
            ImGui::SetNextWindowPos({0, 0});
            ImGui::SetNextWindowSize({1200, 780});
            ImGui::Begin("Catalog test");
            if (frame == 2 || frame == 250) ImGui::OpenPopup("templates.popup");
            if (const auto selected =
                        browser.Draw(entries, "zh-CN", text, host, renderer, frame / 60.0))
                applied = selected;
            if (frame > 20 && renderer.Stats().passes_ > 0) observed_preview = true;
            if (frame == 170) observed_empty = EmptyCatalog();
            if (frame == 5) popup_size = CatalogSize();
            if (frame > 5) {
                const auto current = CatalogSize();
                if (current.x != popup_size->x || current.y != popup_size->y)
                    throw std::runtime_error(
                            "catalog.popup_size_changed: " + std::to_string(popup_size->y) +
                            " -> " + std::to_string(current.y));
            }
            ImGui::End();
            const auto draw = host.EndUi();
            renderer.Submit({}, draw, 0x111822ff);
            if (frame == 120) {
                std::filesystem::create_directories(output);
                const auto path = (output / "catalog").string();
                bgfx::requestScreenShot(BGFX_INVALID_HANDLE, path.c_str());
            }
            renderer.EndFrame();
        }
        if (!observed_preview || !observed_empty || applied)
            throw std::runtime_error("catalog.preview_selection_or_search");
        std::cout << "Catalog: actual click selects and plays a preview without applying a "
                     "template; popup size stays fixed over 300 frames, empty search and reopen\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
