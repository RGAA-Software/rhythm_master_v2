#include <imgui.h>

#include <iostream>
#include <memory>
#include <stdexcept>

#include "curve_editor.h"

namespace {
struct ContextDeleter {
    void operator()(ImGuiContext* context) const { ImGui::DestroyContext(context); }
};
void Check(bool value) {
    if (!value) throw std::runtime_error("curve_ui.contract");
}
}  // namespace
int main() {
    using namespace rhythm;
    try {
        std::unique_ptr<ImGuiContext, ContextDeleter> context(ImGui::CreateContext());
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = {500, 500};
        io.DeltaTime = 1.0f / 60;
        Check(io.Fonts->Build());
        const std::map<std::string, std::string> text{
                {"key_time", "Time"},       {"value", "Value"},          {"curve.step", "Step"},
                {"curve.linear", "Linear"}, {"curve.smooth", "Smooth"},  {"add_key", "Add"},
                {"remove_key", "Remove"},   {"invalid_curve", "Invalid"}};
        parameters::Curve curve;
        ImVec2 add_center{};
        int committed = 0;
        const auto frame = [&] {
            ImGui::NewFrame();
            ImGui::SetNextWindowPos({0, 0});
            ImGui::SetNextWindowSize({500, 500});
            ImGui::Begin("Curve test", nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
            const auto edit = studio::DrawCurveEditor(curve, "curve", text);
            if (edit.changed_ && edit.committed_) ++committed;
            const auto minimum = ImGui::GetItemRectMin();
            const auto maximum = ImGui::GetItemRectMax();
            add_center = {(minimum.x + maximum.x) / 2, (minimum.y + maximum.y) / 2};
            ImGui::End();
            ImGui::Render();
            // Borrow ImGui's draw data only for this synchronous validation call.
            Check(ImGui::GetDrawData()->TotalVtxCount < 10000);
        };
        for (int index = 0; index < 3; ++index) frame();
        io.AddMousePosEvent(add_center.x, add_center.y);
        frame();
        io.AddMouseButtonEvent(0, true);
        frame();
        io.AddMouseButtonEvent(0, false);
        frame();
        Check(curve.Keys().size() == 3 && committed == 1);
        std::vector<parameters::Keyframe> keys;
        for (std::size_t index = 0; index < parameters::Curve::kMaximumKeys; ++index)
            keys.push_back({static_cast<double>(index), static_cast<double>(index % 2)});
        curve.SetKeys(std::move(keys));
        for (int index = 0; index < 3; ++index) frame();
        io.AddMouseButtonEvent(0, true);
        frame();
        io.AddMouseButtonEvent(0, false);
        frame();
        Check(curve.Keys().size() == parameters::Curve::kMaximumKeys && committed == 1);
        std::cout << "curve UI contracts passed: one-command add, key limit and clipped large "
                     "curve\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
