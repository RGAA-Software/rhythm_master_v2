#pragma once

#include "rhythm/editor/history.h"

namespace rhythm::studio {
struct OutputEdit {
    std::optional<editor::Snapshot> committed_{};
    bool preview_changed_ = false;
};
}  // namespace rhythm::studio
