#include "resource_table.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "rhythm/render/budget.h"

namespace rhythm::render::detail {
namespace {
std::atomic<std::uint64_t> next_device{1};
constexpr std::uint64_t kTextureBudget = 256ULL * 1024 * 1024;
}  // namespace
ResourceTable::ResourceTable() : device_(next_device.fetch_add(1)) {}

void ResourceTable::CheckThread() const {
    if (std::this_thread::get_id() != thread_) throw std::logic_error("render.thread_affinity");
}

TextureHandle ResourceTable::Allocate(Extent extent, std::span<const std::uint8_t> rgba,
                                      TexturePrecision precision) {
    CheckReady();
    if (precision > TexturePrecision::kFloat16 ||
        (precision == TexturePrecision::kFloat16 && !rgba.empty()))
        throw std::invalid_argument("render.texture_precision");
    const auto bytes = std::uint64_t{extent.width_} * extent.height_ *
                       (precision == TexturePrecision::kFloat16 ? 8 : 4);
    if (!extent.width_ || !extent.height_ || extent.width_ > 8192 || extent.height_ > 8192 ||
        (!rgba.empty() && rgba.size() != bytes)) {
        throw std::invalid_argument("render.texture_size");
    }
    if (bytes > kTextureBudget - bytes_) throw BudgetExceeded(Budget::kTextureBytes);
    auto slot = std::find_if(slots_.begin(), slots_.end(), [](const Slot& item) {
        return !item.live_ && item.generation_ != std::numeric_limits<std::uint32_t>::max();
    });
    if (slot == slots_.end()) {
        if (slots_.size() >= 1024) throw BudgetExceeded(Budget::kTextureSlots);
        slots_.emplace_back();
        slot = slots_.end() - 1;
    }
    slot->extent_ = extent;
    slot->precision_ = precision;
    slot->live_ = true;
    slot->owners_ = 1;
    slot->render_target_ = rgba.empty();
    slot->depth_ = false;
    slot->depth_texture_ = false;
    slot->sampled_ = false;
    bytes_ += bytes;
    ++live_;
    return {device_, static_cast<std::uint32_t>(slot - slots_.begin()), slot->generation_};
}

TextureHandle ResourceTable::AllocateDepth(Extent extent) {
    const auto handle = Allocate(extent, {}, TexturePrecision::kUnorm8);
    slots_[handle.slot_].depth_texture_ = true;
    slots_[handle.slot_].render_target_ = false;
    return handle;
}
bool ResourceTable::IsDepth(TextureHandle handle) const {
    return IsValid(handle) && slots_[handle.slot_].depth_texture_;
}
void ResourceTable::ValidateSceneDepth(TextureHandle color, TextureHandle depth) const {
    CheckReady();
    if (!IsRenderTarget(color) || !IsDepth(depth) || Size(color) != Size(depth))
        throw std::invalid_argument("render.scene_depth_target");
}
bool ResourceTable::IsValid(TextureHandle handle) const { return Owns(handle) && !lost_; }
bool ResourceTable::Owns(TextureHandle handle) const {
    CheckThread();
    return handle.device_ == device_ && handle.slot_ < slots_.size() &&
           slots_[handle.slot_].live_ && slots_[handle.slot_].generation_ == handle.generation_;
}

void ResourceTable::Retain(TextureHandle handle) {
    CheckReady();
    if (!IsValid(handle)) throw std::invalid_argument("render.stale_texture");
    auto& owners = slots_[handle.slot_].owners_;
    if (owners == std::numeric_limits<std::uint32_t>::max())
        throw std::overflow_error("render.texture_owners");
    ++owners;
}
bool ResourceTable::Release(TextureHandle handle) noexcept {
    if (std::this_thread::get_id() != thread_) std::terminate();
    if (!Owns(handle)) return false;
    auto& slot = slots_[handle.slot_];
    if (--slot.owners_) return false;
    bytes_ -= std::uint64_t{slot.extent_.width_} * slot.extent_.height_ *
              ((slot.precision_ == TexturePrecision::kFloat16 ? 8 : 4) + (slot.depth_ ? 4 : 0));
    slot.live_ = false;
    ++slot.generation_;
    --live_;
    return true;
}

Extent ResourceTable::Size(TextureHandle handle) const {
    if (!IsValid(handle)) throw std::invalid_argument("render.stale_texture");
    return slots_[handle.slot_].extent_;
}
bool ResourceTable::IsRenderTarget(TextureHandle handle) const {
    return IsValid(handle) && slots_[handle.slot_].render_target_;
}
TexturePrecision ResourceTable::Precision(TextureHandle handle) const {
    if (!IsValid(handle)) throw std::invalid_argument("render.stale_texture");
    if (IsDepth(handle)) throw std::invalid_argument("render.depth_not_color");
    return slots_[handle.slot_].precision_;
}
void ResourceTable::ValidateUpload(TextureHandle handle, std::span<const std::uint8_t> rgba) const {
    CheckReady();
    const auto extent = Size(handle);
    if (IsRenderTarget(handle) || IsDepth(handle) ||
        rgba.size() != std::size_t(extent.width_) * extent.height_ * 4)
        throw std::invalid_argument("render.texture_upload");
    if (slots_[handle.slot_].sampled_) throw std::logic_error("render.upload_after_sample");
}
void ResourceTable::BeginFrame() {
    CheckReady();
    for (auto& slot : slots_) slot.sampled_ = false;
}
void ResourceTable::ValidateSceneMaterials(TextureHandle target, const SceneDrawList& list,
                                           TextureHandle depth_target) const {
    CheckReady();
    if (list.environment_) {
        const auto& env = *list.environment_;
        if (!IsValid(env.atlas_) || IsDepth(env.atlas_) || env.atlas_ == target ||
            Size(env.atlas_) != kEnvironmentAtlasExtent ||
            Precision(env.atlas_) != TexturePrecision::kFloat16 || !std::isfinite(env.energy_) ||
            env.energy_ < 0 || env.energy_ > 100 || !std::isfinite(env.rotation_) ||
            std::abs(env.rotation_) > 36000)
            throw std::invalid_argument("render.environment_texture");
    }
    if (list.shadow_) {
        const auto& shadow = *list.shadow_;
        if (!IsDepth(shadow.depth_) || shadow.depth_ == depth_target ||
            Size(shadow.depth_) != Extent{shadow.resolution_, shadow.resolution_})
            throw std::invalid_argument("render.shadow_texture");
    }
    for (const auto& draw : list.draws_)
        for (const auto texture : draw.textures_.slots_)
            if (texture != TextureHandle{} &&
                (!IsValid(texture) || IsDepth(texture) || texture == target))
                throw std::invalid_argument("render.material_texture");
}
void ResourceTable::RecordSceneSamples(const SceneDrawList& list) {
    if (list.environment_) slots_[list.environment_->atlas_.slot_].sampled_ = true;
    if (list.shadow_) slots_[list.shadow_->depth_.slot_].sampled_ = true;
    for (const auto& draw : list.draws_)
        for (const auto texture : draw.textures_.slots_)
            if (IsValid(texture)) slots_[texture.slot_].sampled_ = true;
}
void ResourceTable::RecordGpuPointSamples(TextureHandle target, const GpuPointStyle& style) {
    if (!style.sampling_) return;
    const auto texture = style.sampling_->texture_;
    if (!IsValid(texture) || IsDepth(texture) || texture == target)
        throw std::invalid_argument("render.gpu_point_sampling");
    slots_.at(texture.slot_).sampled_ = true;
}
void ResourceTable::RecordSamples(const DrawList& list) {
    const auto sample = [&](TextureHandle handle) {
        if (IsValid(handle)) slots_[handle.slot_].sampled_ = true;
    };
    for (const auto& command : list.commands_) {
        sample(command.texture_);
        if (command.texture_displace_) sample(command.texture_displace_->map_);
        if (command.texture_trail_) sample(command.texture_trail_->history_);
        if (command.depth_of_field_) sample(command.depth_of_field_->depth_);
    }
}
bool ResourceTable::ReserveDepth(TextureHandle handle) {
    CheckReady();
    if (!IsRenderTarget(handle)) throw std::invalid_argument("render.scene_target");
    auto& slot = slots_[handle.slot_];
    if (slot.depth_) return false;
    const auto bytes = std::uint64_t{slot.extent_.width_} * slot.extent_.height_ * 4;
    if (bytes > kTextureBudget - bytes_) throw BudgetExceeded(Budget::kTextureBytes);
    bytes_ += bytes;
    slot.depth_ = true;
    return true;
}
void ResourceTable::DropDepth(TextureHandle handle) noexcept {
    if (!Owns(handle)) return;
    auto& slot = slots_[handle.slot_];
    if (!slot.depth_) return;
    bytes_ -= std::uint64_t{slot.extent_.width_} * slot.extent_.height_ * 4;
    slot.depth_ = false;
}

void ResourceTable::Validate(TextureHandle target, const DrawList& list) const {
    CheckReady();
    if (target != TextureHandle{} && !IsValid(target)) throw std::invalid_argument("render.target");
    if (IsDepth(target)) throw std::invalid_argument("render.depth_not_color");
    if (!std::isfinite(list.width_) || !std::isfinite(list.height_) || list.width_ <= 0 ||
        list.height_ <= 0 || list.width_ > 65535 || list.height_ > 65535 ||
        list.vertices_.size() > 1000000 || list.indices_.size() > 3000000 ||
        list.commands_.size() > 10000) {
        throw std::invalid_argument("render.draw_budget");
    }
    for (const auto& vertex : list.vertices_) {
        if (!std::isfinite(vertex.x_) || !std::isfinite(vertex.y_) || !std::isfinite(vertex.u_) ||
            !std::isfinite(vertex.v_))
            throw std::invalid_argument("render.vertex");
    }
    for (const auto index : list.indices_) {
        if (index >= list.vertices_.size()) throw std::invalid_argument("render.index");
    }
    for (const auto& command : list.commands_) {
        const auto effects =
                int(command.texture_noise_.has_value()) + int(command.texture_filter_.has_value()) +
                int(command.color_adjustment_.has_value()) +
                int(command.texture_mapping_.has_value()) +
                int(command.texture_contours_.has_value()) +
                int(command.texture_displace_.has_value()) +
                int(command.texture_trail_.has_value()) + int(command.color_pipeline_.has_value()) +
                int(command.depth_linearization_.has_value()) +
                int(command.depth_of_field_.has_value()) +
                int(command.environment_filter_.has_value()) +
                int(command.image_program_.has_value()) + int(command.texture_fxaa_.has_value());
        if (effects > 1) throw std::invalid_argument("render.effect_conflict");
        const auto bounded = [](float value, float minimum, float maximum) {
            return std::isfinite(value) && value >= minimum && value <= maximum;
        };
        if (IsDepth(command.texture_) != command.depth_linearization_.has_value())
            throw std::invalid_argument("render.depth_sampling");
        const auto validate_projection = [&](const DepthLinearization& depth) {
            if (!bounded(depth.near_, 0.001f, 10000) || !bounded(depth.far_, depth.near_, 100000) ||
                depth.far_ <= depth.near_)
                throw std::invalid_argument("render.depth_projection");
        };
        if (command.environment_filter_ &&
            (!IsRenderTarget(target) || Size(target) != kEnvironmentAtlasExtent ||
             Precision(target) != TexturePrecision::kFloat16))
            throw std::invalid_argument("render.environment_target");
        if (command.depth_linearization_) validate_projection(*command.depth_linearization_);
        if (command.depth_of_field_) {
            const auto& dof = *command.depth_of_field_;
            validate_projection(dof.projection_);
            if (!IsDepth(dof.depth_) || dof.depth_ == target || !IsValid(command.texture_) ||
                Size(dof.depth_) != Size(command.texture_) ||
                !bounded(dof.focus_, 0.001f, 100000) || !bounded(dof.focus_scale_, 0, 1000) ||
                !bounded(dof.radius_, 0, 32) || dof.samples_ < 1 || dof.samples_ > 64)
                throw std::invalid_argument("render.depth_of_field");
        }
        if (command.color_pipeline_) {
            const auto& color = *command.color_pipeline_;
            if (color.input_ > ColorTransfer::kSrgb || color.output_ > ColorTransfer::kSrgb ||
                color.tone_mapping_ > ToneMapping::kReinhard || !bounded(color.exposure_, -8, 8))
                throw std::invalid_argument("render.color_pipeline");
        }
        if (command.texture_fxaa_) {
            const auto& fxaa = *command.texture_fxaa_;
            if (!bounded(fxaa.span_, 1, 16) || !bounded(fxaa.reduce_multiplier_, 0.01f, 1) ||
                !bounded(fxaa.reduce_minimum_, 0.001f, 0.25f) || !bounded(fxaa.strength_, 0, 1))
                throw std::invalid_argument("render.texture_fxaa");
        }
        if (command.texture_trail_) {
            const auto& trail = *command.texture_trail_;
            if (!IsValid(trail.history_) || IsDepth(trail.history_) || trail.history_ == target ||
                !bounded(trail.retention_, 0, 1) || !bounded(trail.scale_, 0.5f, 2) ||
                !bounded(trail.rotation_, -180, 180))
                throw std::invalid_argument("render.texture_trail");
        }
        if (command.texture_displace_) {
            const auto& displace = *command.texture_displace_;
            if (!IsValid(displace.map_) || IsDepth(displace.map_) || displace.map_ == target ||
                displace.kind_ > TextureDisplaceKind::kVectorRg ||
                !bounded(displace.strength_, -1, 1) || !bounded(displace.radius_, 1, 32) ||
                !bounded(displace.rotation_, -36000, 36000))
                throw std::invalid_argument("render.texture_displace");
        }
        if (command.texture_mapping_) {
            const auto& mapping = *command.texture_mapping_;
            if (mapping.kind_ > TextureMappingKind::kPolar ||
                !bounded(mapping.scale_, 0.125f, 16) ||
                !bounded(mapping.rotation_, -36000, 36000) ||
                !bounded(mapping.travel_, -4096, 4096) || !bounded(mapping.twist_, -16, 16) ||
                !bounded(mapping.sectors_, 1, 32) ||
                std::floor(mapping.sectors_) != mapping.sectors_ ||
                !bounded(mapping.radial_power_, -2, 4))
                throw std::invalid_argument("render.texture_mapping");
        }
        if (command.texture_contours_) {
            const auto& contours = *command.texture_contours_;
            if (!bounded(contours.count_, 1, 64) || !bounded(contours.width_, 0.01f, 0.49f) ||
                !bounded(contours.phase_, -4096, 4096))
                throw std::invalid_argument("render.texture_contours");
            for (const auto& color : {contours.color_a_, contours.color_b_})
                for (const auto channel : color)
                    if (!bounded(channel, 0, 1))
                        throw std::invalid_argument("render.contour_color");
        }

        if (command.texture_noise_) {
            const auto& noise = *command.texture_noise_;
            if (command.color_adjustment_ || command.texture_filter_ ||
                !std::isfinite(noise.scale_) || noise.scale_ < 0.25f || noise.scale_ > 32 ||
                !std::isfinite(noise.phase_) || noise.phase_ < 0 || noise.phase_ > 4096 ||
                !std::isfinite(noise.contrast_) || noise.contrast_ < 0 || noise.contrast_ > 4 ||
                !std::isfinite(noise.seed_) || noise.seed_ < 0 || noise.seed_ > 1024)
                throw std::invalid_argument("render.texture_noise");
            for (const auto& color : {noise.color_a_, noise.color_b_})
                for (const auto channel : color)
                    if (!std::isfinite(channel) || channel < 0 || channel > 1)
                        throw std::invalid_argument("render.noise_color");
        }
        if (command.texture_filter_) {
            const auto& filter = *command.texture_filter_;
            if (command.color_adjustment_ || filter.kind_ > TextureFilterKind::kDownsample ||
                !std::isfinite(filter.step_x_) || !std::isfinite(filter.step_y_) ||
                filter.step_x_ < 0 || filter.step_x_ > 64 || filter.step_y_ < 0 ||
                filter.step_y_ > 64)
                throw std::invalid_argument("render.texture_filter");
        }
        if (command.color_adjustment_) {
            const auto& color = *command.color_adjustment_;
            if (!std::isfinite(color.exposure_) || !std::isfinite(color.contrast_) ||
                !std::isfinite(color.saturation_) || !std::isfinite(color.invert_) ||
                color.exposure_ < -8 || color.exposure_ > 8 || color.contrast_ < 0 ||
                color.contrast_ > 4 || color.saturation_ < 0 || color.saturation_ > 4 ||
                color.invert_ < 0 || color.invert_ > 1)
                throw std::invalid_argument("render.color_adjustment");
        }
        if (!IsValid(command.texture_) || command.texture_ == target ||
            command.first_index_ > list.indices_.size() ||
            command.index_count_ > list.indices_.size() - command.first_index_ ||
            command.index_count_ % 3 != 0 || !std::isfinite(command.clip_.x_) ||
            !std::isfinite(command.clip_.y_) || !std::isfinite(command.clip_.width_) ||
            !std::isfinite(command.clip_.height_) || command.clip_.width_ < 0 ||
            command.clip_.height_ < 0 || command.blend_ > BlendMode::kInverseAlphaMask) {
            throw std::invalid_argument("render.draw_command");
        }
    }
}

FrameStats ResourceTable::Stats() const {
    CheckThread();
    FrameStats stats;
    stats.live_textures_ = live_;
    stats.texture_bytes_ = bytes_;
    return stats;
}
void ResourceTable::Invalidate() {
    CheckThread();
    lost_ = true;
}
void ResourceTable::CheckReady() const {
    CheckThread();
    if (lost_) throw std::logic_error("render.device_lost");
}
}  // namespace rhythm::render::detail
