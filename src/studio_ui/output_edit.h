#pragma once

#include "rhythm/editor/history.h"
#include "rhythm/graph/components.h"

namespace rhythm::studio {
struct OutputEdit {
    std::optional<editor::Snapshot> committed_{};
    std::optional<graph::NodeId> selected_{};
    std::optional<graph::AuthorNode> open_author_{};
    bool unique_instance_ = false;
    bool preview_changed_ = false;
};
}  // namespace rhythm::studio
