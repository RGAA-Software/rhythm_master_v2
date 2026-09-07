#pragma once

#include "rhythm/graph/registry.h"
#include "rhythm/runtime/signal_previews.h"

namespace rhythm::studio {
struct PortVisual {
    std::uint64_t id_ = 0;
    std::string label_{};
    graph::ValueType type_ = graph::ValueType::kScalar;
    bool bound_ = false;
};
struct NodeVisual {
    std::uint64_t id_ = 0;
    std::string title_{};
    std::vector<PortVisual> inputs_{};
    PortVisual output_{};
    bool preview_enabled_ = false;
    std::uint64_t preview_texture_ = 0;
    std::string preview_waiting_{};
    std::optional<runtime::SignalTrace> preview_signal_{};
};
struct PreviewBounds {
    float x_ = 0;
    float y_ = 0;
    float width_ = 0;
    float height_ = 0;
};

// Private Studio widget adapter. IDs belong to the canvas UI identity space.
PreviewBounds DrawNode(const NodeVisual& node);
void DrawLink(std::uint64_t id, std::uint64_t from, std::uint64_t to, graph::ValueType type);
}  // namespace rhythm::studio
