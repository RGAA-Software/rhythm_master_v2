#pragma once

#include <map>

#include "import_file.h"
#include "rhythm/player/scene_queue.h"

namespace rhythm::android_host {
// Ephemeral performance queue. Preparing a scene never changes the last
// explicitly opened installed package. At most 512 MiB of private copies.
class QueuedImports final {
   public:
    explicit QueuedImports(const std::filesystem::path& cache)
        : cache_(std::filesystem::canonical(cache)) {}
    bool Request(const std::filesystem::path& path, std::string title);
    void Pump(bool can_prepare);
    player::SceneQueue& Queue() { return queue_; }

   private:
    std::filesystem::path cache_{};
    std::map<std::uint64_t, ImportFile> files_{};
    // Destroyed first, joining its worker before releasing any private copies.
    player::SceneQueue queue_{};
};
}  // namespace rhythm::android_host
