#include "image_program_store.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "rhythm/render/budget.h"

namespace rhythm::render::detail {
ImageProgramHandle ImageProgramStore::Allocate(std::span<const std::uint8_t> artifact) {
    if (lost_) throw std::logic_error("render.device_lost");
    if (artifact.size() < 27 || artifact.size() > 512 * 1024 || artifact[0] != 'F' ||
        artifact[1] != 'S' || artifact[2] != 'H' || artifact[3] != 12)
        throw std::invalid_argument("render.image_program_artifact");
    if (artifact.size() > 16 * 1024 * 1024 - bytes_)
        throw BudgetExceeded(Budget::kBackendResources);
    auto found = std::find_if(slots_.begin(), slots_.end(), [](const Slot& slot) {
        return slot.bytes_ == 0 && slot.generation_ != std::numeric_limits<std::uint32_t>::max();
    });
    if (found == slots_.end()) {
        if (slots_.size() >= 64) throw BudgetExceeded(Budget::kBackendResources);
        slots_.emplace_back();
        found = slots_.end() - 1;
    }
    found->bytes_ = std::uint32_t(artifact.size());
    bytes_ += artifact.size();
    return {device_, std::uint32_t(found - slots_.begin()), found->generation_};
}
bool ImageProgramStore::Owns(ImageProgramHandle handle) const {
    return handle.device_ == device_ && handle.slot_ < slots_.size() &&
           slots_[handle.slot_].generation_ == handle.generation_ &&
           slots_[handle.slot_].bytes_ != 0;
}
void ImageProgramStore::Release(ImageProgramHandle handle) noexcept {
    if (!Owns(handle)) return;
    auto& slot = slots_[handle.slot_];
    bytes_ -= slot.bytes_;
    slot.bytes_ = 0;
    ++slot.generation_;
}
void ImageProgramStore::Validate(const ImageProgramInput& input) const {
    if (!IsValid(input.program_) || !std::isfinite(input.seconds_) || input.seconds_ < 0 ||
        input.seconds_ > 1000000 ||
        !std::all_of(input.parameters_.begin(), input.parameters_.end(), [](float value) {
            return std::isfinite(value) && std::abs(value) <= 1000000;
        }))
        throw std::invalid_argument("render.image_program_input");
}
void ImageProgramStore::AddStats(FrameStats& stats) const {
    stats.image_program_bytes_ = bytes_;
    stats.image_programs_ = std::uint32_t(std::count_if(
            slots_.begin(), slots_.end(), [](const auto& slot) { return slot.bytes_ != 0; }));
}
}  // namespace rhythm::render::detail
