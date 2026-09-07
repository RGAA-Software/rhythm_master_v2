#pragma once

#include "rhythm/export/settings.h"
#include "rhythm/render/renderer.h"

namespace rhythm::exporting::detail {
// Owns the encoder on its own thread (including COM/MF initialization). At most
// two queued images plus the active encoder frame; backpressure is offline-only.
class EncodingQueue final {
   public:
    EncodingQueue(std::filesystem::path path, media::EncodingSettings settings,
                  std::stop_token stop);
    ~EncodingQueue();
    EncodingQueue(const EncodingQueue&) = delete;
    EncodingQueue& operator=(const EncodingQueue&) = delete;
    void Push(render::ReadbackImage image, std::vector<float> audio);
    std::uint64_t Completed() const;
    void Finish();

   private:
    class Impl;
    std::unique_ptr<Impl> impl_{};
};
}  // namespace rhythm::exporting::detail
