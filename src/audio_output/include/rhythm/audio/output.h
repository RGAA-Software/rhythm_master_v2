#pragma once

#include <cstdint>
#include <memory>
#include <span>

namespace rhythm::audio {
struct OutputSnapshot {
    std::uint64_t submitted_frames_ = 0;
    std::uint64_t pulled_frames_ = 0;
    std::uint32_t queued_frames_ = 0;
    double device_buffer_seconds_ = 0;
    bool paused_ = true;
};

// A thin device adapter: accepts 48 kHz stereo float PCM, owns no decoder,
// playlist or feature analyzer. All methods are serialized on its owning worker.
// Starts paused. At most 0.5 seconds of input may be queued. pulled_frames marks
// consumption by the device adapter, not a hardware presentation timestamp.
class OutputDevice final {
   public:
    OutputDevice();
    ~OutputDevice();
    OutputDevice(const OutputDevice&) = delete;
    OutputDevice& operator=(const OutputDevice&) = delete;
    // Accepts at most 4096 frames. A full queue returns false without mutation.
    bool Queue(std::span<const float> samples);
    void Pause(bool paused);
    void Clear();
    void FinishInput();
    void SetVolume(float volume);
    OutputSnapshot Snapshot() const;

   private:
    class Impl;
    std::unique_ptr<Impl> impl_{};
};
}  // namespace rhythm::audio
