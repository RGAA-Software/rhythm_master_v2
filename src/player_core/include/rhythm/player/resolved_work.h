#pragma once

#include "rhythm/performance/list.h"
#include "rhythm/storage/file_bytes.h"

namespace rhythm::player {
struct ResolvedWork {
    performance::ListEntry entry_{};
    performance::ResolutionState state_ = performance::ResolutionState::kMissing;
    storage::FileBytes bytes_{};
    std::string error_{};
};
}  // namespace rhythm::player
