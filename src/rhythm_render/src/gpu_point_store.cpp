#include "gpu_point_store.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "rhythm/render/budget.h"

namespace rhythm::render::detail {
GpuPointHandle GpuPointStore::Allocate(std::uint32_t capacity) {
    if (lost_) throw std::logic_error("render.device_lost");
    if (!capacity || capacity > kMaximumGpuPoints)
        throw std::invalid_argument("render.gpu_point_capacity");
    if (capacity > kMaximumGpuPointTotal - total_) throw BudgetExceeded(Budget::kBackendResources);
    auto found = std::find_if(slots_.begin(), slots_.end(), [](const Slot& slot) {
        return slot.capacity_ == 0 && slot.generation_ != std::numeric_limits<std::uint32_t>::max();
    });
    if (found == slots_.end()) {
        if (slots_.size() >= 16) throw BudgetExceeded(Budget::kBackendResources);
        slots_.emplace_back();
        found = slots_.end() - 1;
    }
    found->capacity_ = capacity;
    found->initialized_ = false;
    total_ += capacity;
    return {device_, static_cast<std::uint32_t>(found - slots_.begin()), found->generation_};
}
bool GpuPointStore::Owns(GpuPointHandle handle) const {
    return handle.device_ == device_ && handle.slot_ < slots_.size() &&
           slots_[handle.slot_].generation_ == handle.generation_ && slots_[handle.slot_].capacity_;
}
std::uint32_t GpuPointStore::Capacity(GpuPointHandle handle) const {
    if (!IsValid(handle)) throw std::invalid_argument("render.gpu_point_handle");
    return slots_[handle.slot_].capacity_;
}
void GpuPointStore::Release(GpuPointHandle handle) noexcept {
    if (!Owns(handle)) return;
    auto& slot = slots_[handle.slot_];
    total_ -= slot.capacity_;
    slot.capacity_ = 0;
    slot.initialized_ = false;
    ++slot.generation_;
}
void GpuPointStore::Validate(GpuPointHandle handle, const GpuParticleStep& step) const {
    const auto capacity = Capacity(handle);
    const auto within = [](float value, float minimum, float maximum) {
        return std::isfinite(value) && value >= minimum && value <= maximum;
    };
    if ((!slots_[handle.slot_].initialized_ && !step.reset_) || step.spawn_start_ >= capacity ||
        step.spawn_count_ > capacity || step.sequence_ > 0xffffff || step.seed_ > 0xffffff ||
        !within(step.seconds_, 0, 1.0f / 30) || !within(step.radius_, 0, 4) ||
        !within(step.speed_, 0, 8) || !within(step.drag_, 0, 1) ||
        !within(step.lifetime_, 0.05f, 120) || !within(step.size_, 0, 1) ||
        !within(step.flow_, 0, 8) || !within(step.frequency_, 0, 100) ||
        !within(step.phase_, -10000, 10000))
        throw std::invalid_argument("render.gpu_particle_step");
    for (const auto value : step.center_)
        if (!within(value, -4, 4)) throw std::invalid_argument("render.gpu_particle_center");
    for (const auto value : step.gravity_)
        if (!within(value, -8, 8)) throw std::invalid_argument("render.gpu_particle_gravity");
    for (const auto& color : {step.color_a_, step.color_b_})
        for (const auto value : color)
            if (!within(value, 0, 1)) throw std::invalid_argument("render.gpu_particle_color");
}
void GpuPointStore::Updated(GpuPointHandle handle) { slots_.at(handle.slot_).initialized_ = true; }
void GpuPointStore::ValidateDraw(GpuPointHandle handle, const GpuPointStyle& style) const {
    (void)Capacity(handle);
    if (!slots_[handle.slot_].initialized_ || !std::isfinite(style.opacity_) ||
        style.opacity_ < 0 || style.opacity_ > 1)
        throw std::invalid_argument("render.gpu_point_draw");
}
void GpuPointStore::AddStats(FrameStats& stats) const {
    stats.gpu_point_capacity_ = total_;
    stats.gpu_point_bytes_ = std::uint64_t(total_) * 64;
    stats.gpu_point_buffers_ = static_cast<std::uint32_t>(std::count_if(
            slots_.begin(), slots_.end(), [](const Slot& slot) { return slot.capacity_ != 0; }));
}
}  // namespace rhythm::render::detail
