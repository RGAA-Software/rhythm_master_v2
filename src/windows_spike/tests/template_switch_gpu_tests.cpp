#include <bgfx/bgfx.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <set>
#include <system_error>
#include <utility>

#include "control_delivery_checks.h"
#include "rhythm/project/package.h"
#include "rhythm/project/store.h"
#include "rhythm/studio/studio.h"
#include "workflow_evidence.h"

namespace {
class SuccessfulRunCleanup final {
   public:
    explicit SuccessfulRunCleanup(std::filesystem::path path) : path_(std::move(path)) {}
    ~SuccessfulRunCleanup() {
        if (!successful_) return;
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }
    SuccessfulRunCleanup(const SuccessfulRunCleanup&) = delete;
    SuccessfulRunCleanup& operator=(const SuccessfulRunCleanup&) = delete;
    void MarkSuccessful() { successful_ = true; }

   private:
    std::filesystem::path path_{};
    bool successful_ = false;
};
void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
void Activate(const char* window_name, const char* item) {
    // Checked borrowed ImGui windows remain inside this synchronous test adapter.
    const auto* window = ImGui::FindWindowByName(window_name);
    Check(window != nullptr, "template switch window missing");
    ImGui::ActivateItemByID(ImHashStr(item, 0, window->ID));
}
void PopupAction(const std::string& item, const std::string& child = {}, int index = -1) {
    const auto& context = *ImGui::GetCurrentContext();
    Check(!context.OpenPopupStack.empty(), "template switch popup missing");
    const auto* popup = context.OpenPopupStack.back().Window;
    Check(popup != nullptr, "template switch popup window missing");
    if (child.empty()) {
        ImGui::ActivateItemByID(ImHashStr(item.c_str(), 0, popup->ID));
        return;
    }
    for (const auto* window : context.Windows)
        if (window->ParentWindow == popup &&
            std::string_view(window->Name).find(child) != std::string_view::npos) {
            const auto scope =
                    index < 0 ? window->ID : ImHashData(&index, sizeof(index), window->ID);
            ImGui::ActivateItemByID(ImHashStr(item.c_str(), 0, scope));
            return;
        }
    throw std::runtime_error("template switch child missing");
}
}  // namespace
int main(int argc, char* argv[]) {
    using namespace rhythm;
    try {
        Check(argc == 4 || (argc == 5 && std::string_view(argv[4]) == "--controls") ||
                      (argc == 6 && std::string_view(argv[4]) == "--template"),
              "template_switch resources locale output [--controls|--template name]");
        const bool controls = argc == 5;
        const std::string selected_template = argc == 6 ? argv[5] : "";
        const std::filesystem::path resources(argv[1]);
        const std::string locale(argv[2]);
        const auto root =
                std::filesystem::absolute(argv[3]) /
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        SuccessfulRunCleanup cleanup(root);
        const auto project_path = root / "Projects/switch.rhythmproj";
        const auto package_path = root / "Published/switch.rhythmpack";
        const auto entries = project::ScanTemplates(resources / "content/templates");
        std::ifstream file(resources / "locales" / locale / "studio.json");
        const auto text = nlohmann::json::parse(file).get<std::map<std::string, std::string>>();
        platform::Host host(true);
        host.Resize({1600, 1000});
        ImGui::GetIO().IniFilename = nullptr;
        auto renderer = host.CreateRenderer();
        auto font = host.CreateFontTexture(renderer);
        studio::Studio studio(resources, project_path);
        testing::WorkflowEvidence evidence(root);
        std::cout << "evidence: " << root.string() << std::endl;
        std::string action = "default";
        int frame = 0;
        const auto start = std::chrono::steady_clock::now();
        const auto tick = [&] {
            Check(host.Poll(), "template switch host closed");
            Check(std::chrono::steady_clock::now() - start <
                          std::chrono::seconds(controls ? 180 : 60),
                  "template switch timed out");
            host.BeginUi();
            renderer.BeginFrame();
        };
        const auto finish = [&] {
            studio.Frame(host, renderer, frame++ / 60.0);
            renderer.Submit({}, host.EndUi(), 0x111822ff);
            renderer.EndFrame();
            evidence.Record(action, studio);
        };
        for (int index = 0; index < 180; ++index) {
            tick();
            if (index == 5 && locale == "en-US") Activate("###graph", "###locale");
            finish();
            if (index >= 19 && studio.HasValidPlan()) break;
        }
        Check(studio.HasValidPlan(), "default Studio output not ready");
        // Template application must verify the final output, save/reopen and
        // publication path. Dedicated preview tests cover live thumbnails; turn
        // them off here so a viewport-demand change cannot repeatedly supersede
        // the pending template compilation.
        tick();
        Activate("###graph", "###viewers");
        finish();
        bool selected_found = selected_template.empty();
        for (std::size_t entry_index = 0; entry_index < entries.size(); ++entry_index) {
            const auto& entry = entries[entry_index];
            if (entry.tier_ != "advanced") continue;
            const auto name = entry.directory_.filename().string();
            if (name == selected_template) selected_found = true;
            // The calibration run verifies the editor's public-control workflow
            // against a work that declares controls. The regular delivery run
            // applies every Advanced catalog entry, including new works.
            if (controls && name != "chromatic_loom") continue;
            if (!selected_template.empty() && name != selected_template) continue;
            action = "select:" + name;
            const auto expected = project::LoadRevision(entry.directory_).snapshot_;
            const auto expected_plan = std::get<graph::ExecutionPlan>(
                    graph::Compile(expected.document_, graph::Registry{}));
            std::set<std::string> audio_sources;
            if (expected.soundtrack_)
                for (const auto& clip : expected.soundtrack_->clips_)
                    audio_sources.insert(clip.asset_.sha256_);
            const int selected = static_cast<int>(entry_index);
            bool complete = false;
            for (int step = 0; step < 500 && !complete; ++step) {
                tick();
                auto& io = ImGui::GetIO();
                if (step == 0) Activate("###graph", "###templates");
                if (step == 3) PopupAction("###template.search");
                if (step == 5 || step == 6) {
                    io.AddKeyEvent(ImGuiMod_Ctrl, step == 5);
                    io.AddKeyEvent(ImGuiKey_A, step == 5);
                }
                if (step == 7) io.AddInputCharactersUTF8(entry.titles_.at(locale).c_str());
                if (step == 8 || step == 9) io.AddKeyEvent(ImGuiKey_Enter, step == 8);
                if (step == 12) PopupAction("###title", "catalog.entries", selected);
                if (step == 16) {
                    action = "apply:" + name;
                    PopupAction(text.at("catalog.use"), "catalog.detail");
                }
                if (step > 25 && step % 20 == 10 && studio.HasValidPlan())
                    Activate("###graph", "###save");
                if (step > 45 && step % 20 == 0 && studio.HasValidPlan())
                    Activate("###graph", "###publish");
                finish();
                const bool plan_ready = studio.HasValidPlan();
                const bool node_count_matches =
                        studio.Status().authored_nodes_ == expected.document_.nodes_.size();
                const bool saved_file_exists = std::filesystem::exists(project_path / "CURRENT");
                const bool package_exists = std::filesystem::exists(package_path);
                if (step == 499 &&
                    (!plan_ready || !node_count_matches || !package_exists || !saved_file_exists))
                    std::cerr << name << ": plan=" << plan_ready << " nodes=" << node_count_matches
                              << " package=" << package_exists << " saved=" << saved_file_exists
                              << '\n';
                if (step < 50 || !plan_ready || !node_count_matches || !package_exists ||
                    !saved_file_exists)
                    continue;
                const auto saved = project::Load(project_path).snapshot_;
                const auto published = project::LoadPackage(package_path);
                const bool titles_match =
                        saved.title_ == expected.title_ && published.title_ == expected.title_;
                if (!titles_match) {
                    if (step == 499)
                        std::cerr << name << ": title mismatch, saved=" << saved.title_
                                  << " published=" << published.title_ << '\n';
                    continue;
                }
                Check(saved.document_.output_ != expected.document_.output_,
                      "UI application must remap template identities");
                Check(published.program_.instructions_.size() ==
                                      expected_plan.instructions_.size() &&
                              published.program_.controls_.Definitions().size() ==
                                      expected_plan.controls_.Definitions().size() &&
                              saved.document_.control_cues_ == expected.document_.control_cues_ &&
                              saved.document_.beat_grid_ == expected.document_.beat_grid_ &&
                              published.program_.beat_grid_ == expected.document_.beat_grid_ &&
                              !studio.Status().budget_limited_,
                      "current output/save/publication lost template structure or controls");
                const auto capture = (root / (locale + "-" + name)).string();
                bgfx::requestScreenShot(BGFX_INVALID_HANDLE, capture.c_str());
                for (int settle = 0; settle < 3; ++settle) {
                    tick();
                    finish();
                }
                complete = true;
                std::cout << name << ": real Studio selection, current output, "
                          << audio_sources.size()
                          << " cached clip waveforms, save and publish passed\n";
            }
            Check(complete, "template application did not replace the current output");
            if (controls) {
                action = "controls:" + name;
                testing::CheckControlDelivery(studio, project_path, package_path, root / name, [&] {
                    tick();
                    finish();
                });
            }
        }
        Check(selected_found, "selected template switch fixture missing");
        std::cout << "captures: " << root.string() << '\n';
        cleanup.MarkSuccessful();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
