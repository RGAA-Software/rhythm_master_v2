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
                               std::span<const std::uint32_t> indices,
                               std::span<const SkinWeights> skin,
                               std::span<const MorphTarget> morphs) {
    if (lost_) throw std::logic_error("render.device_lost");
    if (vertices.empty() || vertices.size() > 250000 || indices.empty() ||
        indices.size() > 750000 || indices.size() % 3)
        throw std::invalid_argument("render.mesh_size");
    if (!skin.empty() && skin.size() != vertices.size())
        throw std::invalid_argument("render.skin_vertices");
    std::uint8_t bones = 0;
    for (const auto& vertex : skin) {
        float sum = 0;
        for (std::size_t i = 0; i < 4; ++i) {
            if (vertex.joints_[i] >= kMaximumSkinBones || !std::isfinite(vertex.weights_[i]) ||
                vertex.weights_[i] < 0 || vertex.weights_[i] > 1)
                throw std::invalid_argument("render.skin_weights");
            bones = std::max(bones, static_cast<std::uint8_t>(vertex.joints_[i] + 1));
            sum += vertex.weights_[i];
        }
        if (std::abs(sum - 1) > 1e-4f) throw std::invalid_argument("render.skin_weights");
    }
    if (morphs.size() > 4) throw std::invalid_argument("render.morph_count");
    for (const auto& target : morphs) {
        if (target.deltas_.size() != vertices.size())
            throw std::invalid_argument("render.morph_vertices");
        for (const auto& delta : target.deltas_)
            for (const auto& values : {delta.position_, delta.normal_, delta.tangent_})
                if (!std::all_of(values.begin(), values.end(), Finite))
                    throw std::invalid_argument("render.morph_delta");
    }
    // RGBA32F: three texels per target vertex, rows padded to 1024; lookup/joint
    // stream: 24 bytes per vertex. Both allocations count against mesh storage.
    const auto extra_bytes =
            morphs.empty() ? skin.size_bytes()
                           : ((vertices.size() * morphs.size() * 3 + 1023) / 1024) * 1024 * 16 +
                                     vertices.size() * 24;
    const auto bytes = vertices.size_bytes() + indices.size_bytes() + extra_bytes;
    const bool tangents = std::all_of(vertices.begin(), vertices.end(), [](const auto& v) {
        const auto& t = v.tangent_;
        const auto length = std::hypot(t[0], t[1], t[2]);
        return std::isfinite(length) && length > 0.99f && length < 1.01f && std::abs(t[3]) == 1 &&
               std::abs(t[0] * v.normal_x_ + t[1] * v.normal_y_ + t[2] * v.normal_z_) < 0.01f;
    });
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
    slot->tangents_ = tangents;
    slot->bones_ = bones;
    slot->morphs_ = static_cast<std::uint8_t>(morphs.size());
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
    if (list.lights_.size() + list.positional_lights_.size() > 4 ||
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
    for (const auto& light : list.positional_lights_) {
        const auto bounded = [](float v, float low, float high) {
            return std::isfinite(v) && v >= low && v <= high;
        };
        const auto length =
                std::hypot(light.direction_[0], light.direction_[1], light.direction_[2]);
        if (!std::all_of(light.position_.begin(), light.position_.end(),
                         [&](float v) { return bounded(v, -1000000, 1000000); }) ||
            !std::all_of(light.radiance_.begin(), light.radiance_.end(),
                         [&](float v) { return bounded(v, 0, 100); }) ||
            !bounded(length, 0.99f, 1.01f) || !bounded(light.range_, 0.01f, 10000) ||
            !bounded(light.decay_, 0, 4) || !bounded(light.cone_angle_, 0.1f, 89) ||
            !bounded(light.cone_decay_, 0.1f, 16))
            throw std::invalid_argument("render.positional_light");
    }
    std::uint64_t indices = 0, bone_matrices = 0;
    if (list.shadow_) {
        const auto& shadow = *list.shadow_;
        if (!ValidMatrix(shadow.world_to_clip_) ||
            shadow.light_ >= list.lights_.size() + list.positional_lights_.size() ||
            shadow.resolution_ < 256 || shadow.resolution_ > 2048 ||
            (shadow.resolution_ & (shadow.resolution_ - 1)) != 0 ||
            !std::isfinite(shadow.depth_bias_) || shadow.depth_bias_ < 0 ||
            shadow.depth_bias_ > 0.05f || !std::isfinite(shadow.normal_bias_) ||
            shadow.normal_bias_ < 0 || shadow.normal_bias_ > 1)
            throw std::invalid_argument("render.shadow_settings");
        if (shadow.light_ >= list.lights_.size() &&
            !list.positional_lights_[shadow.light_ - list.lights_.size()].spot_)
            throw std::invalid_argument("render.point_shadow_unsupported");
    }
    for (const auto& draw : list.draws_) {
        if (draw.deformations_.size() > 4) throw std::invalid_argument("render.deformation_limit");
        for (const auto& deformation : draw.deformations_)
            if (!std::isfinite(deformation.twist_) || std::abs(deformation.twist_) > 720 ||
                !std::isfinite(deformation.taper_) || std::abs(deformation.taper_) > 4 ||
                deformation.axis_ > 2 ||
                !std::all_of(deformation.pivot_.begin(), deformation.pivot_.end(), [](float value) {
                    return std::isfinite(value) && std::abs(value) <= 10000;
                }))
                throw std::invalid_argument("render.deformation_parameters");
        if (!std::isfinite(draw.textures_.normal_scale_) || draw.textures_.normal_scale_ < 0 ||
            draw.textures_.normal_scale_ > 4 ||
            !std::all_of(draw.textures_.uv_transform_.begin(), draw.textures_.uv_transform_.end(),
                         [](float v) { return std::isfinite(v) && std::abs(v) <= 10000; }))
            throw std::invalid_argument("render.material_texture_options");
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
        const auto required_bones = slots_[draw.mesh_.slot_].bones_;
        for (std::size_t i = 0; i < draw.morph_weights_.size(); ++i)
            if (!std::isfinite(draw.morph_weights_[i]) || std::abs(draw.morph_weights_[i]) > 100 ||
                (i >= slots_[draw.mesh_.slot_].morphs_ && draw.morph_weights_[i] != 0))
                throw std::invalid_argument("render.morph_weights");
        bone_matrices += draw.bones_.size();
        if (bone_matrices > 65536) throw std::length_error("render.skin_draw_budget");
        if ((required_bones == 0) != draw.bones_.empty() || draw.bones_.size() < required_bones ||
            draw.bones_.size() > kMaximumSkinBones)
            throw std::invalid_argument("render.skin_palette");
        for (const auto& bone : draw.bones_)
            if (!ValidMatrix(bone) || bone[3] != 0 || bone[7] != 0 || bone[11] != 0 ||
                bone[15] != 1)
                throw std::invalid_argument("render.skin_matrix");
        if (draw.textures_.slots_[1].device_ && !slots_[draw.mesh_.slot_].tangents_)
            throw std::invalid_argument("render.material_tangents");
        if (indices > 3000000) throw std::length_error("render.scene_index_budget");
    }
}
void MeshStore::AddStats(FrameStats& stats) const {
    stats.live_meshes_ = live_;
    stats.mesh_bytes_ = bytes_;
}
}  // namespace rhythm::render::detail
