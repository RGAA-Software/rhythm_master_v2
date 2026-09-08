#include "queued_imports.h"

#include <algorithm>

namespace rhythm::android_host {
bool QueuedImports::Request(const std::filesystem::path& path, std::string title) {
    try {
        const auto normalized = std::filesystem::weakly_canonical(path);
        for (const auto& [id, file] : files_)
            if (file.Path() == normalized) return false;
        auto file = ImportFile::Claim(path, cache_);
        if (!file) return false;
        std::uintmax_t bytes = std::filesystem::file_size(file->Path());
        for (const auto& [id, owned] : files_) bytes += std::filesystem::file_size(owned.Path());
        if (bytes > 512ULL * 1024 * 1024) return false;
        const auto id = queue_.Enqueue(file->Path(), std::move(title));
        if (!id) return false;
        files_.emplace(*id, std::move(*file));
        return true;
    } catch (const std::exception&) {
        return false;
    }
}
void QueuedImports::Pump(bool can_prepare) {
    queue_.Pump(false);
    // Collect completion before cleanup, and cleanup before starting a new
    // worker. A removed loading row's source survives until its worker drains.
    if (!queue_.Busy()) {
        const auto items = queue_.Items();
        std::erase_if(files_, [&](const auto& entry) {
            return std::none_of(items.begin(), items.end(),
                                [&](const auto& item) { return item.id_ == entry.first; });
        });
    }
    queue_.Pump(can_prepare);
}
}  // namespace rhythm::android_host
