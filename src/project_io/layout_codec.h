#pragma once

#include <nlohmann/json.hpp>

#include "rhythm/editor/history.h"

namespace rhythm::project {
nlohmann::json EncodeLayout(const editor::Snapshot& snapshot);
void DecodeLayout(const nlohmann::json& layout, editor::Snapshot& snapshot);
}  // namespace rhythm::project
