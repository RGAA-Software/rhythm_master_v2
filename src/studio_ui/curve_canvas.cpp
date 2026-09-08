#include <imgui.h>

#include <algorithm>
#include <cmath>

#include "curve_editor.h"

namespace rhythm::studio {
CurveEdit CurveEditor::DrawPlot(parameters::Curve& curve) {
    CurveEdit edit;
    if (!dragged_) {
        time_min_ = curve.Keys().front().seconds_;
        time_max_ = std::max(time_min_ + 0.01, curve.Keys().back().seconds_);
        value_min_ = value_max_ = curve.Keys().front().value_;
        for (int i = 0; i <= 128; ++i) {
            const auto value = curve.Evaluate(std::lerp(time_min_, time_max_, double(i) / 128));
            value_min_ = std::min(value_min_, value);
            value_max_ = std::max(value_max_, value);
        }
        const auto padding = std::max(0.1, (value_max_ - value_min_) * 0.2);
        value_min_ -= padding;
        value_max_ += padding;
    }
    const auto origin = ImGui::GetCursorScreenPos();
    const ImVec2 extent{std::max(80.0f, ImGui::GetContentRegionAvail().x), 140};
    ImGui::InvisibleButton("##curve", extent, ImGuiButtonFlags_MouseButtonLeft);
    const auto point = [&](double time, double value) {
        return ImVec2{
                origin.x + 10 +
                        float((time - time_min_) / (time_max_ - time_min_)) * (extent.x - 20),
                origin.y + extent.y - 10 -
                        float((value - value_min_) / (value_max_ - value_min_)) * (extent.y - 20)};
    };
    // ImGui owns the draw list; borrow only during this synchronous UI call.
    const auto draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(origin, {origin.x + extent.x, origin.y + extent.y},
                        IM_COL32(16, 24, 32, 255), 4);
    draw->PushClipRect(origin, {origin.x + extent.x, origin.y + extent.y}, true);
    for (int i = 0; i <= 4; ++i) {
        const auto y = origin.y + 10 + (extent.y - 20) * i / 4;
        draw->AddLine({origin.x + 10, y}, {origin.x + extent.x - 10, y}, IM_COL32(42, 54, 66, 255));
    }
    auto last = point(time_min_, curve.Evaluate(time_min_));
    for (int i = 1; i <= 128; ++i) {
        const auto time = std::lerp(time_min_, time_max_, double(i) / 128);
        const auto next = point(time, curve.Evaluate(time));
        draw->AddLine(last, next, IM_COL32(80, 205, 225, 255), 1.5f);
        last = next;
    }
    const auto mouse = ImGui::GetIO().MousePos;
    double best = 64;
    std::optional<std::size_t> hit;
    int hit_handle = 0;
    const auto probe = [&](ImVec2 position, std::size_t index, int handle) {
        const auto dx = position.x - mouse.x, dy = position.y - mouse.y;
        const auto distance = double(dx * dx + dy * dy);
        if (distance < best) {
            best = distance;
            hit = index;
            hit_handle = handle;
        }
    };
    const auto keys = curve.Keys();
    float marker_x = origin.x - 10;
    for (std::size_t i = 0; i < keys.size(); ++i) {
        const auto center = point(keys[i].seconds_, keys[i].value_);
        if (i + 1 == keys.size() || center.x - marker_x >= 6 ||
            (selected_.size() <= 16 && selected_.contains(i))) {
            marker_x = center.x;
            draw->AddCircleFilled(center, selected_.contains(i) ? 5.0f : 3.0f,
                                  selected_.contains(i) ? IM_COL32(255, 192, 70, 255)
                                                        : IM_COL32(160, 215, 235, 255));
        }
        probe(center, i, 0);
        if (!selected_.contains(i) || selected_.size() > 16) continue;
        for (const int side : {-1, 1}) {
            if (side < 0 ? i == 0 ||
                                   keys[i - 1].interpolation_ != parameters::Interpolation::kHermite
                         : i + 1 == keys.size() ||
                                   keys[i].interpolation_ != parameters::Interpolation::kHermite)
                continue;
            const auto neighbor = side < 0 ? keys[i - 1].seconds_ : keys[i + 1].seconds_;
            const auto dt = (neighbor - keys[i].seconds_) / 3;
            const auto slope = side < 0 ? keys[i].in_slope_ : keys[i].out_slope_;
            const auto handle = point(keys[i].seconds_ + dt, keys[i].value_ + slope * dt);
            draw->AddLine(center, handle, IM_COL32(255, 192, 70, 255));
            draw->AddCircle(handle, 4, IM_COL32(255, 192, 70, 255));
            probe(handle, i, side);
        }
    }
    draw->PopClipRect();
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0)) {
        if (!ImGui::GetIO().KeyCtrl && (!hit || !selected_.contains(*hit))) selected_.clear();
        if (hit) {
            if (ImGui::GetIO().KeyCtrl && selected_.contains(*hit))
                selected_.erase(*hit);
            else
                selected_.insert(*hit);
            dragged_ = hit;
            handle_ = hit_handle;
            drag_changed_ = false;
        }
    }
    if (dragged_ && ImGui::IsMouseDragging(0, 2)) {
        const auto i = *dragged_;
        auto changed = std::vector<parameters::Keyframe>(keys.begin(), keys.end());
        auto& key = changed.at(i);
        const auto value = value_min_ + (origin.y + extent.y - 10 - mouse.y) / (extent.y - 20) *
                                                (value_max_ - value_min_);
        if (!handle_) {
            const auto time = time_min_ +
                              (mouse.x - origin.x - 10) / (extent.x - 20) * (time_max_ - time_min_);
            const auto minimum = i ? std::nextafter(keys[i - 1].seconds_, 1e10) : 0;
            const auto maximum =
                    i + 1 < keys.size() ? std::nextafter(keys[i + 1].seconds_, -1.0) : 1e9;
            key.seconds_ = std::clamp(time, minimum, maximum);
            key.value_ = std::clamp(value, -1e12, 1e12);
        } else {
            const auto neighbor = handle_ < 0 ? keys[i - 1].seconds_ : keys[i + 1].seconds_;
            const auto slope =
                    std::clamp((value - key.value_) * 3 / (neighbor - key.seconds_), -1e12, 1e12);
            (handle_ < 0 ? key.in_slope_ : key.out_slope_) = slope;
        }
        if (changed[i] != keys[i]) {
            curve.SetKeys(std::move(changed));
            edit.changed_ = drag_changed_ = true;
        }
    }
    if (dragged_ && ImGui::IsMouseReleased(0)) {
        edit.committed_ = drag_changed_;
        dragged_.reset();
        drag_changed_ = false;
    }
    return edit;
}
bool CurveEditor::DrawBatch(parameters::Curve& curve,
                            const std::map<std::string, std::string>& text) {
    if (!ImGui::CollapsingHeader(text.at("curve.batch").c_str())) return false;
    if (ImGui::Button(text.at("curve.select_all").c_str()))
        for (std::size_t i = 0; i < curve.Keys().size(); ++i) selected_.insert(i);
    ImGui::SameLine();
    if (ImGui::Button(text.at("curve.clear_selection").c_str())) selected_.clear();
    const auto input = [&](const char* label, double& value) {
        ImGui::SetNextItemWidth(110);
        ImGui::InputDouble(text.at(label).c_str(), &value, 0, 0, "%.3f");
    };
    input("curve.time_offset", transform_.time_offset_);
    input("curve.time_scale", transform_.time_scale_);
    input("curve.time_pivot", transform_.time_pivot_);
    input("curve.value_offset", transform_.value_offset_);
    input("curve.value_scale", transform_.value_scale_);
    input("curve.value_pivot", transform_.value_pivot_);
    ImGui::BeginDisabled(selected_.empty());
    const auto apply = ImGui::Button(text.at("curve.apply_selection").c_str());
    ImGui::EndDisabled();
    if (!apply) return false;
    try {
        const std::vector<std::size_t> selected(selected_.begin(), selected_.end());
        auto result = parameters::TransformKeys(curve, selected, transform_);
        const auto changed = result != curve;
        curve = std::move(result);
        selected_.clear();
        transform_ = {};
        error_.clear();
        return changed;
    } catch (const std::exception&) {
        error_ = "invalid_curve";
        return false;
    }
}
}  // namespace rhythm::studio
