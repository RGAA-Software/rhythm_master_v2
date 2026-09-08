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
                {"key_time", "Time"},
                {"value", "Value"},
                {"curve.step", "Step"},
                {"curve.linear", "Linear"},
                {"curve.smooth", "Smooth"},
                {"curve.hermite", "Tangent / Hermite"},
                {"curve.in_slope", "Incoming slope / s"},
                {"curve.out_slope", "Outgoing slope / s"},
                {"curve.batch", "Edit selected keys"},
                {"curve.select_all", "Select all keys"},
                {"curve.clear_selection", "Clear selection"},
                {"curve.time_offset", "Time offset"},
                {"curve.time_scale", "Time scale"},
                {"curve.time_pivot", "Time pivot"},
                {"curve.value_offset", "Value offset"},
                {"curve.value_scale", "Value scale"},
                {"curve.value_pivot", "Value pivot"},
                {"curve.apply_selection", "Apply to selected keys"},
                {"add_key", "Add"},
                {"remove_key", "Remove"},
                {"invalid_curve", "Invalid"}};
        parameters::Curve curve;
        studio::CurveEditor editor;
        ImVec2 add_center{};
        ImVec2 plot_origin{};
        float plot_width = 0;
        int committed = 0;
        const auto frame = [&] {
            ImGui::NewFrame();
            ImGui::SetNextWindowPos({0, 0});
            ImGui::SetNextWindowSize({500, 500});
            ImGui::Begin("Curve test", nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
            plot_origin = ImGui::GetCursorScreenPos();
            plot_width = ImGui::GetContentRegionAvail().x;
            const auto edit = editor.Draw(curve, "curve", text);
            if (edit.committed_) ++committed;
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
        curve = parameters::Curve({{0, 0, parameters::Interpolation::kHermite}, {1, 1}});
        frame();
        const auto click = [&](ImVec2 from, ImVec2 to) {
            io.AddMousePosEvent(from.x, from.y);
            frame();
            io.AddMouseButtonEvent(0, true);
            frame();
            io.AddMousePosEvent(to.x, to.y);
            frame();
            io.AddMouseButtonEvent(0, false);
            frame();
        };
        const auto first_y = plot_origin.y + 130 - 120 * 0.2f / 1.4f;
        click({plot_origin.x + 10, first_y}, {plot_origin.x + 50, first_y - 20});
        Check(committed == 2 && curve.Keys()[0].seconds_ > 0 && curve.Keys()[0].value_ > 0);
        curve = parameters::Curve({{0, 0, parameters::Interpolation::kHermite}, {1, 1}});
        frame();
        click({plot_origin.x + 10, first_y}, {plot_origin.x + 10, first_y});
        const auto handle_x = plot_origin.x + 10 + (plot_width - 20) / 3;
        click({handle_x, first_y}, {handle_x, first_y - 20});
        Check(committed == 3 && curve.Keys()[0].out_slope_ > 0.5 && curve.Keys()[0].seconds_ == 0 &&
              curve.Keys()[0].value_ == 0);
        std::cout << "curve UI contracts passed: one-command add, key limit and clipped large "
                     "curve\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
