#include "rhythm/media/audio_cursor_budget.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace rhythm::media {
class AudioCursorBudget::State final {
   public:
    std::size_t active_ = 0;
    std::size_t peak_ = 0;
};
AudioCursorBudget::AudioCursorBudget() : state_(std::make_shared<State>()) {}
AudioCursorBudget::Lease::Lease(std::shared_ptr<State> state) : state_(std::move(state)) {}
AudioCursorBudget::Lease::~Lease() {
    if (state_) --state_->active_;
}
AudioCursorBudget::Lease::Lease(Lease&& other) noexcept : state_(std::move(other.state_)) {}
AudioCursorBudget::Lease& AudioCursorBudget::Lease::operator=(Lease&& other) noexcept {
    if (this != &other) {
        if (state_) --state_->active_;
        state_ = std::move(other.state_);
    }
    return *this;
}
AudioCursorBudget::Lease AudioCursorBudget::Acquire() const {
    if (state_->active_ >= kMaximum) throw std::length_error("audio.transition_cursor_budget");
    ++state_->active_;
    state_->peak_ = std::max(state_->peak_, state_->active_);
    return Lease(state_);
}
std::size_t AudioCursorBudget::Active() const { return state_->active_; }
std::size_t AudioCursorBudget::Peak() const { return state_->peak_; }
}  // namespace rhythm::media
