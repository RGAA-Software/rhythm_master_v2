#include <bgfx/bgfx.h>
#include <imgui.h>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>

#include "rhythm/platform/host.h"
#include "rhythm/project/store.h"

// Arguments are borrowed only at the process ABI boundary.
int main(int argc, char* argv[]) {
    using namespace rhythm;
    try {
        if (argc != 3) throw std::invalid_argument("template_text.arguments");
        const auto entries = project::ScanTemplates(argv[1]);
        const auto found = std::find_if(entries.begin(), entries.end(), [](const auto& entry) {
            return entry.id_ == "official.templates.prismatic_lotus";
        });
        if (found == entries.end()) throw std::runtime_error("template_text.missing");
        const auto label = found->titles_.at("zh-CN");
        const auto loaded = project::LoadRevision(found->directory_);
        if (label != "棱镜星莲" || loaded.snapshot_.title_ != label + " / Prismatic lotus")
            throw std::runtime_error("template_text.mojibake");
        const std::filesystem::path output(argv[2]);
        std::filesystem::create_directories(output);
        platform::Host host(true);
        auto renderer = host.CreateRenderer();
        auto font = host.CreateFontTexture(renderer);
        for (int frame = 0; frame < 8; ++frame) {
            if (!host.Poll()) throw std::runtime_error("template_text.closed");
            host.BeginUi();
            ImGui::SetNextWindowPos({40, 40}, ImGuiCond_Always);
            ImGui::SetNextWindowSize({900, 260}, ImGuiCond_Always);
            ImGui::Begin("模板名称检查", nullptr, ImGuiWindowFlags_NoSavedSettings);
            ImGui::TextUnformatted("从实际模板目录读取，使用微软雅黑：");
            ImGui::Selectable((label + "###template").c_str());
            ImGui::Text("作品名称：%s", loaded.snapshot_.title_.c_str());
            ImGui::End();
            auto draw = host.EndUi();
            renderer.BeginFrame();
            renderer.Submit({}, draw, 0x111822ff);
            if (frame == 5) {
                const auto path = (output / "template-title").string();
                bgfx::requestScreenShot(BGFX_INVALID_HANDLE, path.c_str());
            }
            renderer.EndFrame();
        }
        std::cout << "Actual template label/title UTF-8 content and GPU rendering passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
