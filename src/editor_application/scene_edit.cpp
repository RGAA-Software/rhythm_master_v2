#include "rhythm/editor/scene_edit.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace rhythm::editor {
SceneEdit::SceneEdit(Snapshot base, graph::NodeId node) : draft_(std::move(base)) {
    const auto inspected = InspectSceneTarget(draft_, node);
    if (std::holds_alternative<graph::Diagnostic>(inspected))
        throw std::invalid_argument(std::get<graph::Diagnostic>(inspected).code_);
    target_ = std::get<SceneTarget>(inspected);
}
bool SceneEdit::Update(const scene::Matrix& local) {
    error_.clear();
    const auto pose = scene::DecomposeEuler(local);
    if (!pose) {
        error_ = "scene_edit.non_trs";
        return false;
    }
    for (const auto value : {pose->scale_.x_, pose->scale_.y_, pose->scale_.z_})
        if (value < .001 - 1e-9 || value > 100 + 1e-5) {
            error_ = "scene_edit.range";
            return false;
        }
    auto& nodes = draft_.document_.nodes_;
    auto& node = *std::find_if(nodes.begin(), nodes.end(),
                               [&](const auto& value) { return value.id_ == target_.node_; });
    const auto current_rotation = scene::ComposeEuler(
            {{},
             {graph::Scalar(node, "rotation_x", 0), graph::Scalar(node, "rotation_y", 0),
              graph::Scalar(node, "rotation_z", 0)},
             {1, 1, 1}});
    const auto next_rotation = scene::ComposeEuler({{}, pose->degrees_, {1, 1, 1}});
    bool same_rotation = true;
    for (std::size_t index = 0; index < current_rotation.values_.size(); ++index)
        same_rotation &=
                std::abs(current_rotation.values_[index] - next_rotation.values_[index]) < 1e-6;
    const auto angle = [&](double value, const std::string& key) {
        if (same_rotation) return graph::Scalar(node, key, 0);
        return value + 360 * std::round((graph::Scalar(node, key, 0) - value) / 360);
    };
    const auto scale = [&](double value, const std::string& key) {
        const auto authored = graph::Scalar(node, key, 1);
        const auto effective = std::clamp(authored * target_.uniform_scale_, .001, 100.0);
        return std::abs(effective - value) < 1e-6 ? authored : value / target_.uniform_scale_;
    };
    const std::map<std::string, double> values{
            {"translate_x", pose->translation_.x_},
            {"translate_y", pose->translation_.y_},
            {"translate_z", pose->translation_.z_},
            {"rotation_x", angle(pose->degrees_.x_, "rotation_x")},
            {"rotation_y", angle(pose->degrees_.y_, "rotation_y")},
            {"rotation_z", angle(pose->degrees_.z_, "rotation_z")},
            {"scale_x", scale(pose->scale_.x_, "scale_x")},
            {"scale_y", scale(pose->scale_.y_, "scale_y")},
            {"scale_z", scale(pose->scale_.z_, "scale_z")}};
    for (const auto& [key, value] : values) {
        const auto valid = key.starts_with("translate_")  ? std::abs(value) <= 1000
                           : key.starts_with("rotation_") ? std::abs(value) <= 36000
                                                          : value >= .001 && value <= 100;
        if (!valid) {
            error_ = "scene_edit.range";
            return false;
        }
    }
    bool changed = false;
    for (const auto& [key, value] : values) {
        const auto before = graph::Scalar(node, key, key.starts_with("scale_") ? 1 : 0);
        if (std::abs(before - value) < 1e-6) continue;
        node.properties_[key] = value;
        changed = true;
    }
    return changed;
}
EditResult SceneEdit::Finish(const Snapshot& current) const {
    if (!error_.empty()) return graph::Diagnostic{error_, target_.node_, {}};
    if (current.document_.id_ != draft_.document_.id_ ||
        current.document_.revision_ != draft_.document_.revision_)
        return graph::Diagnostic{"scene_edit.stale", target_.node_, {}};
    return draft_;
}
}  // namespace rhythm::editor
