#pragma once

#include <cstdint>
#include <span>
#include <stop_token>

#include "rhythm/scene/model.h"

namespace rhythm::model_import {
// Initial self-contained GLB 2.0 profile: static triangles, opaque scalar
// materials and one embedded buffer. No filesystem/network access is performed.
// Unsupported textures, animations, compression and required extensions reject.
// Input <=64 MiB, JSON <=8 MiB/depth 64, parser arena <=64 MiB; output uses the
// scene model budget. Returned values own all geometry and metadata.
scene::Model ReadGlb(std::span<const std::uint8_t> bytes, std::stop_token stop = {});
}  // namespace rhythm::model_import
