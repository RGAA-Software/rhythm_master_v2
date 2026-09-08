#pragma once

#include <cstdint>
#include <span>
#include <stop_token>

#include "rhythm/scene/model.h"

namespace rhythm::model_import {
// Initial self-contained GLB 2.0 profile: static triangles, opaque PBR
// materials with embedded PNG/JPEG, UV0 and repeat/bilinear sampling and one embedded buffer. No
// filesystem/network access is performed. Unsupported texture profiles, animations, compression and
// required extensions reject. Input <=64 MiB, JSON <=8 MiB/depth 64, parser arena <=64 MiB; output
// uses the scene model budget. Returned values own geometry, metadata and at most 64 MiB of decoded
// images.
scene::Model ReadGlb(std::span<const std::uint8_t> bytes, std::stop_token stop = {});
}  // namespace rhythm::model_import
