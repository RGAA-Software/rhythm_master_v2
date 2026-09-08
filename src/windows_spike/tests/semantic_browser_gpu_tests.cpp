#include <bgfx/bgfx.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

#include "catalog_preview.h"
#include "semantic_palette.h"

namespace {
void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
void CheckWorkerCapacity() {
    // The existing global budget is eight pools. Idle catalog objects and a
    // prepared preview must leave these slots available for authoring/export.
    std::vector<std::unique_ptr<rhythm::foundation::BlockingExecutor>> pools;
    for (int index = 0; index < 8; ++index)
        pools.push_back(std::make_unique<rhythm::foundation::BlockingExecutor>(
                rhythm::foundation::BlockingOptions{1, 1}));
    rhythm::studio::CatalogPreview first, second;
}
void ChildAction(std::string_view child, const std::string& item) {
    // Checked borrowed ImGui window, confined to the synchronous test boundary.
    for (const auto* window : ImGui::GetCurrentContext()->Windows)
        if ((window->Active || window->WasActive) &&
            std::string_view(window->Name).find(child) != std::string_view::npos) {
            ImGui::ActivateItemByID(ImHashStr(item.c_str(), 0, window->ID));
            return;
        }
    throw std::runtime_error("semantic child missing");
}
ImVec2 PopupSize() {
    const auto& stack = ImGui::GetCurrentContext()->OpenPopupStack;
    Check(!stack.empty() && stack.back().Window, "semantic popup missing");
    return stack.back().Window->Size;
}
}  // namespace
int main(int argc, char* argv[]) {
    using namespace rhythm;
    try {
        Check(argc == 4 || argc == 5, "semantic_browser resources locale output [content id]");
        const std::filesystem::path resources(argv[1]), output(argv[3]);
        graph::Registry registry;
        const auto entries = content::LoadSemantics(resources / "content/semantic", registry);
        Check(!entries.empty(), "semantic catalog required");
        std::size_t selected = 0;
        if (argc == 5) {
            const auto found = std::find_if(entries.begin(), entries.end(), [&](const auto& entry) {
                return entry.metadata_.id_ == argv[4];
            });
            Check(found != entries.end(), "requested semantic entry exists");
            selected = std::size_t(found - entries.begin());
        }
        std::ifstream file(resources / "locales" / argv[2] / "studio.json");
        const auto text = nlohmann::json::parse(file).get<std::map<std::string, std::string>>();
        platform::Host host(true);
        host.Resize({1200, 900});
        ImGui::GetIO().IniFilename = nullptr;
        auto renderer = host.CreateRenderer();
        auto font = host.CreateFontTexture(renderer);
        studio::SemanticPalette browser;
        CheckWorkerCapacity();
        const auto baseline = renderer.Stats().texture_bytes_;
        int ready = -1;
        int inserted = -1;
        std::optional<ImVec2> size;
        for (int frame = 0; frame < 300; ++frame) {
            Check(host.Poll(), "semantic host closed");
            host.BeginUi();
            renderer.BeginFrame();
            host.ClearViewerTextures();
            ImGui::SetNextWindowPos({0, 0});
            ImGui::SetNextWindowSize({1200, 900});
            ImGui::Begin("Semantic browser test");
            if (frame == 2) ImGui::OpenPopup("semantic.palette");
            if (frame == 8)
                ChildAction("semantic.entries", "###" + entries[selected].metadata_.id_);
            if (ready >= 0 && frame == ready + 30)
                ChildAction("semantic.detail", "###semantic.insert");
            if (const auto selection =
                        browser.Draw(entries, argv[2], text, host, renderer, frame / 60.0, {})) {
                Check(ready >= 0 && frame >= ready + 30 && frame <= ready + 32 &&
                              *selection == selected,
                      "selection only inserts after explicit button");
                editor::Snapshot graph;
                graph.document_.id_ = "browser-insertion";
                const auto edit =
                        content::AddSemantic(graph, entries[*selection], registry, {0, 0}, 1);
                Check(std::holds_alternative<editor::Snapshot>(edit), "semantic insertion command");
                editor::History history(graph);
                Check(history.Apply(std::get<editor::Snapshot>(edit), graph.document_.revision_),
                      "semantic history apply");
                Check(history.Current().document_.nodes_.size() == 1 && history.Undo() &&
                              history.Current().document_.nodes_.empty(),
                      "semantic insertion undo");
                inserted = frame;
            }
            if (frame > 8 && ready < 0 && renderer.Stats().passes_ > 0) {
                ready = frame;
                CheckWorkerCapacity();
            }
            if (frame == 5) size = PopupSize();
            if (frame > 5 && inserted < 0) {
                const auto current = PopupSize();
                Check(current.x == size->x && current.y == size->y, "stable semantic popup");
            }
            ImGui::End();
            const auto preview_stats = renderer.Stats();
            renderer.Submit({}, host.EndUi(), 0x111822ff);
            if (ready >= 0 && frame == ready + 15) {
                std::filesystem::create_directories(output);
                const auto path = (output / argv[2]).string();
                bgfx::requestScreenShot(BGFX_INVALID_HANDLE, path.c_str());
            }
            renderer.EndFrame();
            if (inserted >= 0 && frame > inserted + 5) {
                Check(preview_stats.passes_ == 0 && preview_stats.texture_bytes_ == baseline,
                      "closed browser releases GPU previews");
                break;
            }
        }
        Check(ready >= 0 && inserted >= 0, "semantic preview and insertion complete");
        std::cout << "semantic selection, GPU preview, fixed popup, explicit insertion/undo and "
                     "close pass\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
