#include "mesh_store.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace rhythm::render::detail {
namespace {
bool Finite(float value) { return std::isfinite(value) && std::abs(value) <= 1e6f; }
bool ValidMatrix(const Matrix4& matrix) {
    return std::all_of(matrix.begin(), matrix.end(), Finite);
}
}  // namespace
MeshHandle MeshStore::Allocate(std::span<const MeshVertex> vertices,
                               std::span<const std::uint32_t> indices) {
    if (lost_) throw std::logic_error("render.device_lost");
    if (vertices.empty() || vertices.size() > 250000 || indices.empty() ||
        indices.size() > 750000 || indices.size() % 3)
        throw std::invalid_argument("render.mesh_size");
    const auto bytes = vertices.size_bytes() + indices.size_bytes();
    if (bytes > 128ULL * 1024 * 1024 - bytes_) throw std::length_error("render.mesh_budget");
    for (const auto& vertex : vertices)
        if (!Finite(vertex.x_) || !Finite(vertex.y_) || !Finite(vertex.z_) ||
            !Finite(vertex.normal_x_) || !Finite(vertex.normal_y_) || !Finite(vertex.normal_z_) ||
            !Finite(vertex.u_) || !Finite(vertex.v_))
            throw std::invalid_argument("render.mesh_vertex");
    for (const auto index : indices)
        if (index >= vertices.size()) throw std::invalid_argument("render.mesh_index");
    auto slot = std::find_if(slots_.begin(), slots_.end(), [](const Slot& value) {
        return !value.live_ && value.generation_ != std::numeric_limits<std::uint32_t>::max();
    });
    if (slot == slots_.end()) {
        if (slots_.size() >= 1024) throw std::length_error("render.mesh_limit");
        slots_.emplace_back();
        slot = slots_.end() - 1;
    }
    slot->live_ = true;
    slot->bytes_ = bytes;
    slot->indices_ = static_cast<std::uint32_t>(indices.size());
    bytes_ += bytes;
    ++live_;
    return {device_, static_cast<std::uint32_t>(slot - slots_.begin()), slot->generation_};
}
bool MeshStore::Owns(MeshHandle handle) const {
    return handle.device_ == device_ && handle.slot_ < slots_.size() &&
           slots_[handle.slot_].live_ && slots_[handle.slot_].generation_ == handle.generation_;
}
void MeshStore::Release(MeshHandle handle) noexcept {
    if (!Owns(handle)) return;
    auto& slot = slots_[handle.slot_];
    bytes_ -= slot.bytes_;
    --live_;
    slot.live_ = false;
    ++slot.generation_;
}
void MeshStore::Validate(const SceneDrawList& list) const {
    if (lost_) throw std::logic_error("render.device_lost");
    if (!ValidMatrix(list.view_) || !ValidMatrix(list.projection_) || list.draws_.size() > 16384)
        throw std::invalid_argument("render.scene_budget");
    if (list.lights_.size() > 4 ||
        !std::all_of(list.camera_position_.begin(), list.camera_position_.end(), Finite))
        throw std::invalid_argument("render.scene_lights");
    const auto view_length = std::hypot(list.camera_backward_[0], list.camera_backward_[1],
                                        list.camera_backward_[2]);
    if (!std::isfinite(view_length) || view_length < 0.99f || view_length > 1.01f)
        throw std::invalid_argument("render.scene_view_direction");
    for (const auto& light : list.lights_) {
        const auto length =
                std::hypot(light.direction_[0], light.direction_[1], light.direction_[2]);
        if (!std::isfinite(length) || length < 0.99f || length > 1.01f ||
            !std::all_of(light.radiance_.begin(), light.radiance_.end(),
                         [](float v) { return std::isfinite(v) && v >= 0 && v <= 100; }))
            throw std::invalid_argument("render.scene_light");
    }
    std::uint64_t indices = 0;
    for (const auto& draw : list.draws_) {
        if (!ValidMatrix(draw.normal_) || !std::isfinite(draw.metallic_) || draw.metallic_ < 0 ||
            draw.metallic_ > 1 || !std::isfinite(draw.roughness_) || draw.roughness_ < 0 ||
            draw.roughness_ > 1 ||
            !std::all_of(draw.emissive_.begin(), draw.emissive_.end(),
                         [](float v) { return std::isfinite(v) && v >= 0 && v <= 1000; }))
            throw std::invalid_argument("render.scene_material");
        if (!IsValid(draw.mesh_) || !ValidMatrix(draw.model_) ||
            !std::all_of(draw.color_.begin(), draw.color_.end(),
                         [](float v) { return std::isfinite(v) && v >= 0 && v <= 1; }))
            throw std::invalid_argument("render.scene_draw");
        indices += slots_[draw.mesh_.slot_].indices_;
        if (indices > 3000000) throw std::length_error("render.scene_index_budget");
    }
}
void MeshStore::AddStats(FrameStats& stats) const {
    stats.live_meshes_ = live_;
    stats.mesh_bytes_ = bytes_;
}
}  // namespace rhythm::render::detail
