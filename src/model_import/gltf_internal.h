#pragma once

#include <cgltf.h>

#include "rhythm/model_import/gltf.h"

namespace rhythm::model_import::detail {
void Require(bool condition, const char* message = "gltf.invalid");
void Preflight(std::span<const std::uint8_t> bytes);
void ValidateProfile(const cgltf_data& data);
scene::Mesh ReadMesh(const cgltf_data& data, const cgltf_primitive& primitive,
                     std::size_t& vertices, std::size_t& indices, std::stop_token stop);
}  // namespace rhythm::model_import::detail
