#pragma once

#include <cstddef>
#include <memory>

namespace rhythm::media {
// Copies share one budget. All access is serialized on the owning audio worker.
// Counts logical PCM cursors, including mixer clips, not device or codec threads.
class AudioCursorBudget final {
   private:
    class State;

   public:
    class Lease final {
       public:
        Lease() = default;
        ~Lease();
        Lease(Lease&& other) noexcept;
        Lease& operator=(Lease&& other) noexcept;
        Lease(const Lease&) = delete;
        Lease& operator=(const Lease&) = delete;
        bool Valid() const { return bool(state_); }

       private:
        friend class AudioCursorBudget;
        explicit Lease(std::shared_ptr<State> state);
        std::shared_ptr<State> state_{};
    };
    AudioCursorBudget();
    AudioCursorBudget(const AudioCursorBudget&) = default;
    AudioCursorBudget& operator=(const AudioCursorBudget&) = default;
    Lease Acquire() const;
    std::size_t Active() const;
    std::size_t Peak() const;
    static constexpr std::size_t kMaximum = 4;

   private:
    std::shared_ptr<State> state_{};
};
}  // namespace rhythm::media
