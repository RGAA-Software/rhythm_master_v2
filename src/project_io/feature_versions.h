#pragma once

#include <algorithm>

#include "rhythm/graph/compiler.h"

namespace rhythm::project::detail {
inline bool HasText(std::span<const graph::Node> nodes) {
    return std::any_of(nodes.begin(), nodes.end(), [](const auto& node) {
        return node.type_ == "texture.text" ||
               std::any_of(node.properties_.begin(), node.properties_.end(),
                           [](const auto& property) {
                               return std::holds_alternative<std::string>(property.second);
                           });
    });
}
inline bool HasText(const graph::Document& document) {
    return HasText(document.nodes_) ||
           std::any_of(document.components_.begin(), document.components_.end(),
                       [](const auto& component) { return HasText(component.nodes_); });
}
inline bool HasEvents(std::span<const graph::Node> nodes) {
    return std::any_of(nodes.begin(), nodes.end(),
                       [](const auto& node) { return node.type_.starts_with("event."); });
}
inline bool HasEvents(const graph::Document& document) {
    return HasEvents(document.nodes_) ||
           std::any_of(document.components_.begin(), document.components_.end(),
                       [](const auto& component) { return HasEvents(component.nodes_); });
}
inline std::uint32_t ProgramAbi(const graph::ExecutionPlan& plan) {
    if (std::any_of(plan.instructions_.begin(), plan.instructions_.end(),
                    [](const auto& instruction) {
                        return instruction.operation_ == graph::Operation::kTextureText;
                    }))
        return 6;
    if (std::any_of(plan.instructions_.begin(), plan.instructions_.end(),
                    [](const auto& instruction) {
                        return instruction.operation_ >= graph::Operation::kEventBeat &&
                               instruction.operation_ <= graph::Operation::kEventInput;
                    }))
        return 5;
    return plan.beat_grid_ ? 4 : plan.control_sequence_ ? 3 : 2;
}
}  // namespace rhythm::project::detail
